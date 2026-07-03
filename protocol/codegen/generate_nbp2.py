#!/usr/bin/env python3
"""Generate the first NBP/2 protocol artifacts from protocol/nbp2.yaml.

This is intentionally small and schema-shaped. S1.7 starts by locking the
wire envelope (message ids, frame layout, CRC32, timing-safe token compare);
message payload structs/CBOR are added in the next S1.7 slice.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / "protocol" / "nbp2.yaml"


@dataclass(frozen=True)
class Message:
    name: str
    msg_id: int


@dataclass(frozen=True)
class Schema:
    major: int
    minor: int
    sof: int
    max_ctrl_payload: int
    messages: tuple[Message, ...]


def parse_schema(path: Path) -> Schema:
    text = path.read_text(encoding="utf-8")

    version = re.search(r"version:\s*\{\s*major:\s*(\d+),\s*minor:\s*(\d+)\s*\}", text)
    sof = re.search(r"sof:\s*(0x[0-9A-Fa-f]+|\d+)", text)
    max_ctrl = re.search(r"max_ctrl_payload:\s*(\d+)", text)
    if version is None or sof is None or max_ctrl is None:
        raise SystemExit(f"{path}: schema incompleto para codegen S1.7")

    messages: list[Message] = []
    current_name: str | None = None
    for line in text.splitlines():
        name_match = re.match(r"\s*-\s+name:\s*([A-Z0-9_]+)", line)
        if name_match:
            current_name = name_match.group(1)
            continue

        id_match = re.match(r"\s*id:\s*(0x[0-9A-Fa-f]+|\d+)", line)
        if id_match and current_name is not None:
            messages.append(Message(current_name, int(id_match.group(1), 0)))
            current_name = None

    if not messages:
        raise SystemExit(f"{path}: nenhuma mensagem encontrada")

    seen_ids: dict[int, str] = {}
    for message in messages:
        previous = seen_ids.get(message.msg_id)
        if previous is not None:
            raise SystemExit(
                f"{path}: id duplicado 0x{message.msg_id:04x}: {previous}/{message.name}"
            )
        seen_ids[message.msg_id] = message.name

    return Schema(
        major=int(version.group(1)),
        minor=int(version.group(2)),
        sof=int(sof.group(1), 0),
        max_ctrl_payload=int(max_ctrl.group(1)),
        messages=tuple(messages),
    )


def write_c_header(schema: Schema, out_dir: Path) -> None:
    enum_lines = "\n".join(
        f"    NBP2_MSG_{message.name} = 0x{message.msg_id:04X}u,"
        for message in schema.messages
    )
    (out_dir / "nbp2.h").write_text(
        f"""#ifndef NB_NBP2_H
#define NB_NBP2_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {{
#endif

#define NBP2_PROTO_MAJOR {schema.major}u
#define NBP2_PROTO_MINOR {schema.minor}u
#define NBP2_SOF 0x{schema.sof:02X}u
#define NBP2_MAX_CTRL_PAYLOAD {schema.max_ctrl_payload}u
#define NBP2_FRAME_OVERHEAD 13u

typedef enum {{
{enum_lines}
}} nbp2_msg_type_t;

typedef enum {{
    NBP2_OK = 0,
    NBP2_ERR_INVALID_ARG,
    NBP2_ERR_BUFFER_TOO_SMALL,
    NBP2_ERR_BAD_SOF,
    NBP2_ERR_BAD_CRC,
    NBP2_ERR_PAYLOAD_TOO_LARGE,
}} nbp2_status_t;

typedef struct {{
    uint16_t type;
    uint32_t seq;
    const uint8_t *payload;
    uint16_t payload_len;
}} nbp2_frame_view_t;

uint32_t nbp2_crc32(const uint8_t *data, size_t len);

nbp2_status_t nbp2_encode_frame(uint16_t type,
                                uint32_t seq,
                                const uint8_t *payload,
                                uint16_t payload_len,
                                uint8_t *out_frame,
                                size_t out_capacity,
                                size_t *out_len);

nbp2_status_t nbp2_decode_frame(const uint8_t *frame,
                                size_t frame_len,
                                nbp2_frame_view_t *out_view);

bool nbp2_timing_safe_equal(const uint8_t *left,
                            size_t left_len,
                            const uint8_t *right,
                            size_t right_len);

#ifdef __cplusplus
}}
#endif

#endif
""",
        encoding="utf-8",
    )


def write_c_source(out_dir: Path) -> None:
    (out_dir / "nbp2.c").write_text(
        r'''#include "nbp2.h"

#include <string.h>

static void nbp2_write_u16_le(uint8_t *out, uint16_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
}

static void nbp2_write_u32_le(uint8_t *out, uint32_t value)
{
    out[0] = (uint8_t)(value & 0xFFu);
    out[1] = (uint8_t)((value >> 8u) & 0xFFu);
    out[2] = (uint8_t)((value >> 16u) & 0xFFu);
    out[3] = (uint8_t)((value >> 24u) & 0xFFu);
}

static uint16_t nbp2_read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8u);
}

static uint32_t nbp2_read_u32_le(const uint8_t *data)
{
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) | ((uint32_t)data[3] << 24u);
}

uint32_t nbp2_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFu;
    size_t i;

    if (data == NULL && len > 0u) {
        return 0u;
    }

    for (i = 0u; i < len; ++i) {
        uint8_t bit;

        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            uint32_t mask = 0u - (crc & 1u);
            crc = (crc >> 1u) ^ (0xEDB88320u & mask);
        }
    }

    return ~crc;
}

nbp2_status_t nbp2_encode_frame(uint16_t type,
                                uint32_t seq,
                                const uint8_t *payload,
                                uint16_t payload_len,
                                uint8_t *out_frame,
                                size_t out_capacity,
                                size_t *out_len)
{
    const size_t total_len = NBP2_FRAME_OVERHEAD + (size_t)payload_len;
    uint32_t crc;

    if (out_frame == NULL || out_len == NULL || (payload == NULL && payload_len > 0u)) {
        return NBP2_ERR_INVALID_ARG;
    }
    if (payload_len > NBP2_MAX_CTRL_PAYLOAD) {
        return NBP2_ERR_PAYLOAD_TOO_LARGE;
    }
    if (out_capacity < total_len) {
        return NBP2_ERR_BUFFER_TOO_SMALL;
    }

    out_frame[0] = NBP2_SOF;
    nbp2_write_u16_le(&out_frame[1], payload_len);
    nbp2_write_u16_le(&out_frame[3], type);
    nbp2_write_u32_le(&out_frame[5], seq);
    if (payload_len > 0u) {
        memcpy(&out_frame[9], payload, payload_len);
    }

    crc = nbp2_crc32(&out_frame[1], 8u + payload_len);
    nbp2_write_u32_le(&out_frame[9u + payload_len], crc);
    *out_len = total_len;
    return NBP2_OK;
}

nbp2_status_t nbp2_decode_frame(const uint8_t *frame,
                                size_t frame_len,
                                nbp2_frame_view_t *out_view)
{
    uint16_t payload_len;
    uint32_t expected_crc;
    uint32_t actual_crc;
    size_t expected_len;

    if (frame == NULL || out_view == NULL) {
        return NBP2_ERR_INVALID_ARG;
    }
    if (frame_len < NBP2_FRAME_OVERHEAD) {
        return NBP2_ERR_INVALID_ARG;
    }
    if (frame[0] != NBP2_SOF) {
        return NBP2_ERR_BAD_SOF;
    }

    payload_len = nbp2_read_u16_le(&frame[1]);
    if (payload_len > NBP2_MAX_CTRL_PAYLOAD) {
        return NBP2_ERR_PAYLOAD_TOO_LARGE;
    }
    expected_len = NBP2_FRAME_OVERHEAD + (size_t)payload_len;
    if (frame_len != expected_len) {
        return NBP2_ERR_INVALID_ARG;
    }

    expected_crc = nbp2_read_u32_le(&frame[9u + payload_len]);
    actual_crc = nbp2_crc32(&frame[1], 8u + payload_len);
    if (expected_crc != actual_crc) {
        return NBP2_ERR_BAD_CRC;
    }

    out_view->type = nbp2_read_u16_le(&frame[3]);
    out_view->seq = nbp2_read_u32_le(&frame[5]);
    out_view->payload = &frame[9];
    out_view->payload_len = payload_len;
    return NBP2_OK;
}

bool nbp2_timing_safe_equal(const uint8_t *left,
                            size_t left_len,
                            const uint8_t *right,
                            size_t right_len)
{
    size_t max_len = left_len > right_len ? left_len : right_len;
    size_t i;
    uint8_t diff = (uint8_t)(left_len ^ right_len);

    if ((left == NULL && left_len > 0u) || (right == NULL && right_len > 0u)) {
        return false;
    }

    for (i = 0u; i < max_len; ++i) {
        uint8_t l = i < left_len ? left[i] : 0u;
        uint8_t r = i < right_len ? right[i] : 0u;
        diff |= (uint8_t)(l ^ r);
    }

    return diff == 0u;
}
''',
        encoding="utf-8",
    )


def write_python(schema: Schema, out_dir: Path) -> None:
    constants = "\n".join(
        f"MSG_{message.name} = 0x{message.msg_id:04X}" for message in schema.messages
    )
    (out_dir / "nbp2.py").write_text(
        f'''"""Generated NBP/2 framing helpers. Do not edit by hand."""

from __future__ import annotations

from dataclasses import dataclass
import zlib


PROTO_MAJOR = {schema.major}
PROTO_MINOR = {schema.minor}
SOF = 0x{schema.sof:02X}
MAX_CTRL_PAYLOAD = {schema.max_ctrl_payload}
FRAME_OVERHEAD = 13

{constants}


@dataclass(frozen=True)
class FrameView:
    msg_type: int
    seq: int
    payload: bytes


def crc32(data: bytes) -> int:
    return zlib.crc32(data) & 0xFFFFFFFF


def encode_frame(msg_type: int, seq: int, payload: bytes = b"") -> bytes:
    if len(payload) > MAX_CTRL_PAYLOAD:
        raise ValueError("payload too large")
    header = (
        bytes([SOF])
        + len(payload).to_bytes(2, "little")
        + msg_type.to_bytes(2, "little")
        + seq.to_bytes(4, "little")
    )
    crc = crc32(header[1:] + payload).to_bytes(4, "little")
    return header + payload + crc


def decode_frame(frame: bytes) -> FrameView:
    if len(frame) < FRAME_OVERHEAD:
        raise ValueError("frame too short")
    if frame[0] != SOF:
        raise ValueError("bad sof")
    payload_len = int.from_bytes(frame[1:3], "little")
    if payload_len > MAX_CTRL_PAYLOAD:
        raise ValueError("payload too large")
    expected_len = FRAME_OVERHEAD + payload_len
    if len(frame) != expected_len:
        raise ValueError("bad length")
    expected_crc = int.from_bytes(frame[9 + payload_len : 13 + payload_len], "little")
    actual_crc = crc32(frame[1 : 9 + payload_len])
    if expected_crc != actual_crc:
        raise ValueError("bad crc")
    return FrameView(
        msg_type=int.from_bytes(frame[3:5], "little"),
        seq=int.from_bytes(frame[5:9], "little"),
        payload=frame[9 : 9 + payload_len],
    )


def timing_safe_equal(left: bytes, right: bytes) -> bool:
    max_len = max(len(left), len(right))
    diff = len(left) ^ len(right)
    for index in range(max_len):
        l_val = left[index] if index < len(left) else 0
        r_val = right[index] if index < len(right) else 0
        diff |= l_val ^ r_val
    return diff == 0
''',
        encoding="utf-8",
    )


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--schema", type=Path, default=SCHEMA)
    parser.add_argument("--out", type=Path, default=ROOT / "protocol" / "generated")
    args = parser.parse_args()

    schema = parse_schema(args.schema)
    c_dir = args.out / "c"
    py_dir = args.out / "python"
    c_dir.mkdir(parents=True, exist_ok=True)
    py_dir.mkdir(parents=True, exist_ok=True)

    write_c_header(schema, c_dir)
    write_c_source(c_dir)
    write_python(schema, py_dir)
    print(f"nbp2-codegen: {len(schema.messages)} mensagens geradas em {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
