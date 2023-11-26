import os
import unittest

import steganography


class SteganographyTest(unittest.TestCase):
    def test_normal(self):
        reserved = 130
        data_len = 23576
        msg = os.urandom(data_len + reserved)

        with open("test_image.png", "rb") as src, open("embedded.png", "wb") as dst:
            obj = steganography.Steganography(reserved)
            obj.add_image(src, dst)
            obj.precompute(data_len)
            obj.embed([msg])
            obj.clear()

        with open("embedded.png", "rb") as src:
            extracted = steganography.Steganography.extract(src, reserved)

        self.assertEqual(extracted, msg)

    def test_oversize(self):
        reserved = 130
        data_len = 9999999
        msg = os.urandom(data_len + reserved)

        try:
            with open("test_image.png", "rb") as src, open("embedded.png", "wb") as dst:
                obj = steganography.Steganography(reserved)
                obj.add_image(src, dst)
                obj.precompute(data_len)
                obj.embed([msg])
                obj.clear()
        except ValueError as e:
            print(e)
            return

        self.fail("no error raised on oversized data")

    def test_invalid_image(self):
        reserved = 130

        try:
            with open("test_image.png", "rb") as src:
                steganography.Steganography.extract(src, reserved)
        except ValueError as e:
            print(e)
            return

        self.fail("no error raised on invalid image")


if __name__ == '__main__':
    unittest.main()
