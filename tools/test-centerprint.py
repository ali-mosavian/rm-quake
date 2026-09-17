"""Regression checks for centerprint text preserved by the asset converter."""

import re
import struct
import sys

from mkassets import ent_msg
from mkassets import message_pool


def bsp_messages(path: str) -> list[bytes]:
    bsp = open(path, "rb").read()
    offset, size = struct.unpack_from("<ii", bsp, 4)
    text = bsp[offset : offset + size].split(b"\0")[0].decode("latin1")
    pairs = re.compile(r'"([^"]*)"\s*"([^"]*)"')
    return [
        ent_msg(values)
        for block in text.split("{")[1:]
        if "message" in (values := dict(pairs.findall(block.split("}")[0])))
    ]


def main() -> None:
    raw = "first line\\n\\nfifth line after blanks"
    assert ent_msg({"message": raw}) == b"first line\n\nfifth line after blanks"
    offsets, pool = message_pool([b"one", b"two\nlines", b"one", b""])
    assert pool == b"\0one\0two\nlines\0"
    assert pool[offsets[b"two\nlines"]:] == b"two\nlines\0"

    try:
        ent_msg({"message": "x" * 1024})
    except ValueError:
        pass
    else:
        raise AssertionError("1024-byte centerprint was not rejected")

    if len(sys.argv) > 1:
        messages = bsp_messages(sys.argv[1])
        episode = b"This is the second episode:\nThe Realm of Black Magic\n\nAncient castles and\nstrange beasts ahead..."
        assert episode in messages
        offsets, pool = message_pool(messages)
        assert pool[offsets[episode] :].startswith(episode + b"\0")


if __name__ == "__main__":
    main()
