import sys
import os
import click
import config
import platform
import ast
import os.path
from typing import Sequence

import encryption
import steganography
import distribution


class PythonLiteralOption(click.Option):
    def type_cast_value(self, ctx, value):
        try:
            return ast.literal_eval(value)
        except Exception as e:
            raise click.BadParameter(f"{value}: Invalid literal: {e}")


@click.group
def cli():
    pass


@cli.command(help='Get the version and platform')
def version():
    click.echo(f'SteganoChat CLI, version {config.VERSION}, {platform.platform()}')


@cli.command(help='Compose a message')
@click.option('--contacts', default="./contacts.db", help='Optional, specify the contacts file')
@click.option('--key', default=None, help='Required, specify the key')
@click.option('--images', cls=PythonLiteralOption, help='Required, images to embed in')
@click.option('--file', default=None, help='Optional, path to the embedded file')
@click.option('--user-id', default=None, help='Optional, must be specified unless \'--user-name\' is provided')
@click.option('--user-name', default=None, help='Optional, must be specified unless \'--user-id\' is provided')
@click.option('--output-dir', default="./embedded/", help='Optional, directory to save the embedded file')
@click.option('--image-format', default='PNG', help="Optional, the format of the output images. Defaults to 'PNG'")
def compose(contacts: str, key: str, images: any, filename: str | None, user_id: str | None, user_name: str | None, output_dir: str, image_format: str):
    if not isinstance(key, str):
        raise click.BadParameter("'key' must be a string")

    if not isinstance(images, Sequence):
        raise click.BadParameter("'images' must be a sequence")

    if not isinstance(output_dir, str):
        raise click.BadParameter("'output_dir' must be a string")

    if not isinstance(image_format, str):
        raise click.BadParameter("'image_format' must be a string")

    if filename is not None:
        if not isinstance(filename, str):
            raise click.BadParameter("'file' must be a string")
        with open(filename, "rb") as file:
            content = file.read()
    else:
        content = sys.stdin.read()
        if len(content) == 0:
            raise click.exceptions.FileError(f'no input file provided')

    if not isinstance(contacts, str):
        raise click.BadParameter("'contacts' must be a string")
    contacts_io = open(contacts, "rb")

    encryption_obj = encryption.Encryption(contacts_io, key)

    if user_id is not None:
        if not isinstance(user_id, str):
            raise click.BadParameter("'user_id' must be a string")
    else:
        if user_name is None:
            raise click.BadParameter("'user_id' or 'user_name' must be specified")
        if not isinstance(user_name, str):
            raise click.BadParameter("'user_name' must be a string")
        user_id = encryption_obj.contacts.find_by_name(user_name).id

    embed_obj = steganography.Steganography(config.RESERVED_SIZE)

    try:
        for image_name in images:
            target_name = os.path.join(output_dir, os.path.basename(image_name))
            with open(image_name, "rb") as src, open(target_name, "wb") as dst:
                embed_obj.add_image(src, dst)
    except Exception as e:
        encryption_obj.close()
        embed_obj.clear()
        raise e

    embed_obj.precompute(len(content))
    lengths = embed_obj.precomputed.get_lengths(config.RESERVED_SIZE)

    plain_pieces = distribution.split(content, lengths)
    encrypted_pieces = []
    for piece in plain_pieces:
        encrypted_pieces.append(encryption_obj.send(piece, user_id))

    embed_obj.embed(encrypted_pieces, image_format)

    encryption_obj.close()
    embed_obj.clear()


if __name__ == '__main__':
    sys.stdin = os.fdopen(sys.stdin.fileno(), 'rb', 0)
    cli()
