import copy
import io
import os
import secrets
import unittest

import encryption


class KeySetTest(unittest.TestCase):
    def test(self):
        key_set = encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM)))
        serialized = bytes(key_set)
        recovered = encryption.KeySet.load(serialized)
        self.assertEqual(key_set, recovered)


class KeySetsTest(unittest.TestCase):
    def test_normal(self):
        key_sets = encryption.KeySets(
            encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM))),
            encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM))),
            encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM)))
        )
        serialized = bytes(key_sets)
        recovered = encryption.KeySets.load(serialized)
        self.assertEqual(key_sets, recovered)

    def test_invitation(self):
        key_sets = encryption.KeySets(
            encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM))),
            None,
            None
        )
        serialized = bytes(key_sets)
        recovered = encryption.KeySets.load(serialized)
        self.assertEqual(key_sets, recovered)

    def test_received_invitation(self):
        key_sets = encryption.KeySets(
            encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM))),
            encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM))),
            None
        )
        serialized = bytes(key_sets)
        recovered = encryption.KeySets.load(serialized)
        self.assertEqual(key_sets, recovered)


class UserTest(unittest.TestCase):
    def test(self):
        user = encryption.User.create("TestUser", secrets.randbits(64),
                                      tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM)))
        self.assertEqual(user.status, encryption.UserStatus.InvitationSent)
        user.keys.crt = encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM)))
        self.assertEqual(user.status, encryption.UserStatus.InvitationReceived)
        user.keys.pst = encryption.KeySet.generate(tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM)))
        self.assertEqual(user.status, encryption.UserStatus.Normal)

        serialized = bytes(user)
        recovered = encryption.User.load(serialized)
        self.assertEqual(user, recovered)

        user.keys.new = None
        self.assertEqual(user.status, encryption.UserStatus.Invalid)


class ContactsTest(unittest.TestCase):
    def test(self):
        file = io.BytesIO()
        key = "Hello, World"

        contacts = encryption.Contacts.create(file, key)

        user = contacts.invite("TestUser1")
        self.assertEqual(user, contacts.find_by_id(user.id))
        self.assertEqual((user, True), contacts.find_by_dynamic_id(user.keys.new.dynamic_ids[0]))
        self.assertEqual(None, contacts.find_by_id(user.id + 1))
        self.assertEqual((None, False), contacts.find_by_dynamic_id(user.keys.new.dynamic_ids[0][:1]))

        contacts.receive_invitation("TestUser2", encryption.KeySet.generate(
            tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM))))

        normal_user = copy.deepcopy(user)
        normal_user.keys.pst = encryption.KeySet.generate(
            tuple(os.urandom(8) for _ in range(encryption.DYNAMIC_ID_NUM)))
        contacts.update_user(normal_user)
        self.assertEqual(normal_user.keys.pst, contacts.find_by_id(normal_user.id).keys.pst)

        self.assertEqual((normal_user, False), contacts.find_by_dynamic_id(normal_user.keys.pst.dynamic_ids[0]))
        self.assertEqual((None, False), contacts.find_by_dynamic_id(normal_user.keys.pst.dynamic_ids[0][:1]))

        nonexistent_user = copy.deepcopy(normal_user)
        nonexistent_user.id += 1
        self.assertRaises(encryption.UserNotFound, contacts.update_user, nonexistent_user)

        contacts.save(file)
        file.seek(0)
        recovered = encryption.Contacts.open(file, key)
        self.assertEqual(recovered, contacts)


class EncryptionTest(unittest.TestCase):
    def test(self):
        file1 = io.BytesIO()
        file2 = io.BytesIO()
        key = "Hello, World"
        word = b"Hi!"

        with encryption.Encryption(file1, key, True) as e1:
            with encryption.Encryption(file2, key, True) as e2:
                invitation, user2 = e1.invite("TestUser2", key)
                user1 = e2.receive_invitation(invitation, "TestUser1", key)

                self.assertRaises(encryption.InvitationNotConfirmed, e1.send, word, user2.id)

                msg = e2.send(word, user1.id)
                received_word, received_user = e1.receive(msg)
                self.assertEqual(received_word, word)
                self.assertEqual(received_user.id, user2.id)

                msg = e1.send(word, user2.id)
                received_word, received_user = e2.receive(msg)
                self.assertEqual(received_word, word)
                self.assertEqual(received_user.id, user1.id)


if __name__ == '__main__':
    unittest.main()
