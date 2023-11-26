import base64
import os
import secrets
from dataclasses import dataclass
from enum import Enum
from io import BytesIO
from typing import Self, BinaryIO, Sequence

from cryptography.fernet import Fernet
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.asymmetric import rsa
from cryptography.hazmat.primitives.ciphers.aead import AESCCM

# encryption macros
AES_SIZE = 32  # the size of AES key
NONCE_SIZE = 8  # the size of nonce (IV)
RSA_SIZE = 512  # the size of RSA key
SHA_SIZE = 32  # the size of SHA hash value
ID_SIZE = 8  # the size of user IDs
DYNAMIC_ID_SIZE = 8  # the size of dynamic IDs
DYNAMIC_ID_NUM = 32  # the number of dynamic IDs exchanged in each communication


@dataclass
class KeySet:
    aes_key: bytes  # 32 bytes
    rsa_key: bytes  # 2347 bytes
    dynamic_ids: Sequence[bytes]  # 32 * 8 bytes

    def __bytes__(self) -> bytes:
        return self.aes_key + b"".join(self.dynamic_ids) + self.rsa_key

    def __eq__(self, other: Self) -> bool:
        return self.aes_key == other.aes_key and self.rsa_key == other.rsa_key and self.dynamic_ids == other.dynamic_ids

    @classmethod
    def load(cls, data: bytes) -> Self:
        dynamic_id_bytes: bytes = data[AES_SIZE:AES_SIZE + DYNAMIC_ID_NUM * 8]
        dynamic_ids: tuple = tuple([dynamic_id_bytes[i: i + 8] for i in range(0, DYNAMIC_ID_NUM * 8, 8)])
        return KeySet(aes_key=data[:AES_SIZE], dynamic_ids=dynamic_ids, rsa_key=data[AES_SIZE + DYNAMIC_ID_NUM * 8:])

    @classmethod
    def generate(cls, dynamic_ids: Sequence[bytes]) -> Self:
        aes_key = os.urandom(AES_SIZE)
        rsa_key = rsa.generate_private_key(65537, RSA_SIZE * 8).private_bytes(
            encoding=serialization.Encoding.DER,
            format=serialization.PrivateFormat.TraditionalOpenSSL,
            encryption_algorithm=serialization.NoEncryption()
        )
        return cls(aes_key, rsa_key, dynamic_ids)

    @property
    def public_key(self) -> bytes:
        return serialization.load_der_private_key(self.rsa_key, None).public_key().public_bytes(
            encoding=serialization.Encoding.DER,
            format=serialization.PublicFormat.SubjectPublicKeyInfo
        )


@dataclass
class KeySets:
    new: KeySet | None  # New key set
    crt: KeySet | None  # Current key set
    pst: KeySet | None  # Past key set

    def __bytes__(self) -> bytes:
        new = bytes(self.new) if self.new is not None else b""
        crt = bytes(self.crt) if self.crt is not None else b""
        pst = bytes(self.pst) if self.pst is not None else b""
        return b"".join((
            len(new).to_bytes(2, "little", signed=False), new,
            len(crt).to_bytes(2, "little", signed=False), crt,
            len(pst).to_bytes(2, "little", signed=False), pst
        ))

    def __eq__(self, other: Self) -> bool:
        return self.new == other.new and self.crt == other.crt and self.pst == other.pst

    @classmethod
    def load(cls, data: bytes) -> Self:
        reader = BytesIO(data)
        new_len = int.from_bytes(reader.read(2), "little", signed=False)
        new = KeySet.load(reader.read(new_len)) if new_len != 0 else None
        crt_len = int.from_bytes(reader.read(2), "little", signed=False)
        crt = KeySet.load(reader.read(crt_len)) if crt_len != 0 else None
        pst_len = int.from_bytes(reader.read(2), "little", signed=False)
        pst = KeySet.load(reader.read(pst_len)) if pst_len != 0 else None
        return cls(new, crt, pst)

    @classmethod
    def create(cls, dynamic_ids: Sequence[bytes]) -> Self:
        return cls(KeySet.generate(dynamic_ids), None, None)


class UserStatus(Enum):
    Normal = 0
    InvitationSent = 1
    InvitationReceived = 2
    Invalid = 3


@dataclass
class User:
    id: int
    name: str
    keys: KeySets

    def __bytes__(self) -> bytes:
        name = self.name.encode()
        key_sets_bytes = bytes(self.keys)
        return b"".join((
            self.id.to_bytes(ID_SIZE, "little", signed=False),
            len(name).to_bytes(2, "little", signed=False), name,
            len(key_sets_bytes).to_bytes(2, "little"), key_sets_bytes
        ))

    def __eq__(self, other: Self) -> bool:
        return self.id == other.id and self.name == other.name and self.keys == other.keys

    @classmethod
    def load(cls, data: bytes) -> Self:
        reader = BytesIO(data)
        id_ = int.from_bytes(reader.read(ID_SIZE), "little", signed=False)
        name_len = int.from_bytes(reader.read(2), "little", signed=False)
        name = reader.read(name_len).decode()
        key_sets_len = int.from_bytes(reader.read(2), "little", signed=False)
        key_sets = KeySets.load(reader.read(key_sets_len))
        return cls(id_, name, key_sets)

    @classmethod
    def create(cls, name: str, id_: int, dynamic_ids: Sequence[bytes], crt: KeySet | None = None) -> Self:
        key_sets = KeySets.create(dynamic_ids)
        key_sets.crt = crt
        return cls(id_, name, key_sets)

    @property
    def status(self) -> UserStatus:
        if self.keys.new is not None and self.keys.crt is not None and self.keys.pst is not None:
            return UserStatus.Normal
        elif self.keys.new is not None and self.keys.crt is None and self.keys.pst is None:
            return UserStatus.InvitationSent
        elif self.keys.new is not None and self.keys.crt is not None and self.keys.pst is None:
            return UserStatus.InvitationReceived
        else:
            return UserStatus.Invalid


@dataclass
class Contacts:
    users: list[User]
    key: str

    def __bytes__(self) -> bytes:
        h = hashes.Hash(hashes.SHA256())
        h.update(self.key.encode())
        f = Fernet(base64.urlsafe_b64encode(h.finalize()))

        bytes_list = [len(self.users).to_bytes(2, "little", signed=False)]
        for user in self.users:
            user_bytes = bytes(user)
            bytes_list.append(len(user_bytes).to_bytes(2, "little", signed=False))
            bytes_list.append(user_bytes)

        plain = b"".join(bytes_list)
        cipher = f.encrypt(plain)

        return cipher

    def __eq__(self, other: Self) -> bool:
        return self.users == other.users and self.key == other.key

    @classmethod
    def load(cls, data: bytes, key: str) -> Self:
        h = hashes.Hash(hashes.SHA256())
        h.update(key.encode())
        f = Fernet(base64.urlsafe_b64encode(h.finalize()))

        users = []
        plain = f.decrypt(data)
        reader = BytesIO(plain)

        users_len = int.from_bytes(reader.read(2), "little", signed=False)
        for _ in range(users_len):
            user_len = int.from_bytes(reader.read(2), "little", signed=False)
            users.append(User.load(reader.read(user_len)))

        return Contacts(users, key)

    def find_by_id(self, id_: int) -> User | None:
        for user in self.users:
            if user.id == id_:
                return user
        return None

    def find_by_dynamic_id(self, dynamic_id: bytes) -> tuple[User | None, bool]:
        for user in self.users:
            if user.keys.pst is not None and dynamic_id in user.keys.pst.dynamic_ids:
                return user, False
            if dynamic_id in user.keys.new.dynamic_ids:
                return user, True
        return None, False

    def update_user(self, updated_user: User) -> None:
        for i, user in enumerate(self.users):
            if user.id == updated_user.id:
                self.users[i] = updated_user
                return
        raise ValueError(f"user with id {updated_user.id} not found")

    def invite(self, name: str) -> User:
        user = User.create(name, self.generate_id(), self.generate_dynamic_ids())
        self.users.append(user)
        return user

    def receive_invitation(self, name: str, crt: KeySet) -> User:
        user = User.create(name, self.generate_id(), self.generate_dynamic_ids(), crt)
        self.users.append(user)
        return user

    @classmethod
    def open(cls, file: BinaryIO, key: str) -> Self:
        data = file.read()
        return cls.load(data, key)

    @classmethod
    def create(cls, file: BinaryIO, key: str) -> Self:
        file.seek(0)
        file.truncate()
        contacts = cls([], key)
        file.write(bytes(contacts))
        return contacts

    def save(self, file: BinaryIO) -> None:
        file.seek(0)
        file.truncate()
        file.write(bytes(self))

    def generate_dynamic_ids(self) -> tuple[bytes]:
        dynamic_ids = []
        for _ in range(DYNAMIC_ID_NUM):
            while True:
                id_ = os.urandom(DYNAMIC_ID_SIZE)
                found = True
                if id_ in dynamic_ids:
                    continue
                for user in self.users:
                    if (id_ in user.keys.new.dynamic_ids or
                            (user.keys.pst is not None and id_ in user.keys.pst.dynamic_ids)):
                        found = False
                        break
                if found:
                    dynamic_ids.append(id_)
                    break
        return tuple(dynamic_ids)

    def generate_id(self) -> int:
        while True:
            id_ = secrets.randbits(ID_SIZE * 8)
            found = True
            for user in self.users:
                if id_ == user.id:
                    found = False
                    break
            if found:
                return id_


class Encryption:
    __contacts: Contacts
    __file: BinaryIO
    __closed: bool

    def __init__(self, contacts: BinaryIO, key: str, create: bool = False):
        self.__file = contacts
        if create:
            self.__contacts = Contacts.create(contacts, key)
        else:
            self.__contacts = Contacts.open(contacts, key)
        self.__closed = False

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    def close(self) -> None:
        if self.__closed:
            return
        self.save()
        self.__file.close()
        self.__closed = True

    def save(self) -> None:
        if self.__closed:
            raise ValueError("contacts has been closed")
        self.__contacts.save(self.__file)

    def invite(self, name: str, key: str) -> tuple[bytes, User]:
        if self.__closed:
            raise ValueError("contacts has been closed")

        user = self.__contacts.invite(name)
        plain = bytes(KeySet(user.keys.new.aes_key, user.keys.new.public_key, user.keys.new.dynamic_ids))

        h = hashes.Hash(hashes.SHA256())
        h.update(key.encode())
        f = Fernet(base64.urlsafe_b64encode(h.finalize()))

        return f.encrypt(plain), user

    def receive_invitation(self, cipher: bytes, name: str, key: str) -> User:
        if self.__closed:
            raise ValueError("contacts has been closed")

        h = hashes.Hash(hashes.SHA256())
        h.update(key.encode())
        f = Fernet(base64.urlsafe_b64encode(h.finalize()))

        plain = f.decrypt(cipher, None)
        crt = KeySet.load(plain)

        return self.__contacts.receive_invitation(name, crt)

    def send(self, data: bytes, id_: int) -> bytes:
        if self.__closed:
            raise ValueError("contacts has been closed")

        user = self.__contacts.find_by_id(id_)
        if user is None:
            raise ValueError(f"user {id_} not found")
        match user.status:
            case UserStatus.Normal:
                return self.__encrypt(data, user)
            case UserStatus.InvitationSent:
                raise PermissionError(f"the invitation of user {id_} is not confirmed")
            case UserStatus.InvitationReceived:
                return self.__encrypt(data, user)
            case UserStatus.Invalid:
                raise ValueError(f"invalid user {id_} since its key sets are invalid")

    @classmethod
    def __encrypt(cls, data: bytes, user: User) -> bytes:
        dynamic_id = secrets.choice(user.keys.crt.dynamic_ids)
        nonce = os.urandom(NONCE_SIZE)

        exchange_section_key = os.urandom(AES_SIZE)

        exchange_section_plain = user.keys.new.aes_key + b"".join(user.keys.new.dynamic_ids) + user.keys.new.public_key
        exchange_section_hash = hashes.Hash(hashes.SHA256())
        exchange_section_hash.update(exchange_section_plain)
        exchange_section_ccm = AESCCM(exchange_section_key)
        exchange_section_cipher = exchange_section_ccm.encrypt(nonce, exchange_section_plain, None)
        exchange_section_cipher = exchange_section_cipher + exchange_section_hash.finalize()
        del exchange_section_plain, exchange_section_hash, exchange_section_ccm

        public_key = serialization.load_der_public_key(user.keys.crt.rsa_key)
        exchange_section_len = (len(exchange_section_cipher) - SHA_SIZE).to_bytes(2, "little", signed=False)
        exchange_section_key_cipher = public_key.encrypt(exchange_section_key + exchange_section_len, padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA256()),
            algorithm=hashes.SHA256(),
            label=None
        ))
        exchange_section_cipher = exchange_section_key_cipher + exchange_section_cipher
        del public_key, exchange_section_len, exchange_section_key, exchange_section_key_cipher

        body_hash = hashes.Hash(hashes.SHA256())
        body_hash.update(data)
        body_ccm = AESCCM(user.keys.crt.aes_key)
        body = body_ccm.encrypt(nonce, data, None) + body_hash.finalize()
        del body_ccm, body_hash

        return b"".join((dynamic_id, nonce, exchange_section_cipher, body))

    def receive(self, cipher: bytes) -> tuple[bytes, User]:
        if self.__closed:
            raise ValueError("contacts has been closed")

        reader = BytesIO(cipher)

        dynamic_id = reader.read(DYNAMIC_ID_SIZE)
        user, updated = self.__contacts.find_by_dynamic_id(dynamic_id)
        if user is None:
            raise ValueError(f"user not found: dynamic_id {dynamic_id.hex()} is not found")
        del dynamic_id

        nonce = reader.read(NONCE_SIZE)

        private_key = serialization.load_der_private_key(user.keys.new.rsa_key, None)
        exchange_section_key_cipher = reader.read(RSA_SIZE)
        exchange_section_key_plain = private_key.decrypt(exchange_section_key_cipher, padding.OAEP(
            mgf=padding.MGF1(algorithm=hashes.SHA256()),
            algorithm=hashes.SHA256(),
            label=None
        ))
        del private_key, exchange_section_key_cipher

        exchange_section_key = exchange_section_key_plain[:AES_SIZE]
        exchange_section_len = int.from_bytes(exchange_section_key_plain[AES_SIZE:], "little", signed=False)
        del exchange_section_key_plain

        exchange_section_ccm = AESCCM(exchange_section_key)
        exchange_section_cipher = reader.read(exchange_section_len)
        exchange_section_plain = exchange_section_ccm.decrypt(nonce, exchange_section_cipher, None)
        del exchange_section_ccm, exchange_section_cipher

        expected_exchange_section_hash = hashes.Hash(hashes.SHA256())
        expected_exchange_section_hash.update(exchange_section_plain)
        received_exchange_section_hash = reader.read(SHA_SIZE)
        if expected_exchange_section_hash.finalize() != received_exchange_section_hash:
            raise ValueError("exchange section hash not matched")
        del expected_exchange_section_hash, received_exchange_section_hash

        if updated:
            exchange_section_reader = BytesIO(exchange_section_plain)
            new_aes_key = exchange_section_reader.read(AES_SIZE)
            new_dynamic_ids = tuple([exchange_section_reader.read(DYNAMIC_ID_SIZE) for _ in range(DYNAMIC_ID_NUM)])
            new_public_key = exchange_section_reader.read()
            del exchange_section_reader

            user.keys.pst = user.keys.new
            user.keys.crt = KeySet(new_aes_key, new_public_key, new_dynamic_ids)
            user.keys.new = KeySet.generate(self.__contacts.generate_dynamic_ids())
            del new_aes_key, new_dynamic_ids, new_public_key
        del exchange_section_plain

        data_cipher = reader.read()
        data_ccm = AESCCM(user.keys.pst.aes_key)
        data_plain = data_ccm.decrypt(nonce, data_cipher[:len(data_cipher) - SHA_SIZE], None)
        del data_ccm

        expected_data_hash = hashes.Hash(hashes.SHA256())
        expected_data_hash.update(data_plain)
        received_data_hash = data_cipher[len(data_cipher) - SHA_SIZE:]
        if received_data_hash != expected_data_hash.finalize():
            raise ValueError("data hash not matched")
        del expected_data_hash, received_data_hash, data_cipher

        return data_plain, user
