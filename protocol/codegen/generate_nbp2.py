#!/usr/bin/env python3
"""Generate NBP/2 protocol artifacts from protocol/nbp2.yaml.

S1.7: envelope (message ids, frame layout, CRC32, timing-safe token compare)
plus per-message structs and CBOR encode/decode, for C and Python. This
script is the only place allowed to know the wire format — generated code
is never hand-edited (P4).
"""

from __future__ import annotations

import argparse
import keyword
from dataclasses import dataclass
from pathlib import Path

import yaml


ROOT = Path(__file__).resolve().parents[2]
SCHEMA = ROOT / "protocol" / "nbp2.yaml"

# Tipos escalares suportados hoje pelo nbp2.yaml (não adicionar tipo sem uso real).
SCALAR_C_TYPE = {
    "u8": "uint8_t",
    "u16": "uint16_t",
    "u32": "uint32_t",
    "u64": "uint64_t",
    "i8": "int8_t",
    "f32": "float",
}


@dataclass(frozen=True)
class Field:
    name: str
    type: str
    max: int | None = None
    enum: str | None = None


@dataclass(frozen=True)
class Message:
    name: str
    msg_id: int
    fields: tuple[Field, ...]


@dataclass(frozen=True)
class Schema:
    major: int
    minor: int
    sof: int
    max_ctrl_payload: int
    enums: dict[str, tuple[str, ...]]
    messages: tuple[Message, ...]


def parse_schema(path: Path) -> Schema:
    raw = yaml.safe_load(path.read_text(encoding="utf-8"))

    protocol = raw["protocol"]
    enums = {name: tuple(values) for name, values in raw.get("enums", {}).items()}

    messages: list[Message] = []
    seen_ids: dict[int, str] = {}
    for entry in raw["messages"]:
        fields = tuple(
            Field(
                name=f["name"],
                type=f["type"],
                max=f.get("max"),
                enum=f.get("enum"),
            )
            for f in entry.get("fields", [])
        )
        for field in fields:
            if field.type == "enum" and field.enum not in enums:
                raise SystemExit(
                    f"{path}: {entry['name']}.{field.name}: enum '{field.enum}' não existe"
                )
            if field.type in ("bytes", "str") and not field.max:
                raise SystemExit(
                    f"{path}: {entry['name']}.{field.name}: tipo '{field.type}' exige 'max'"
                )
            if field.type not in SCALAR_C_TYPE and field.type not in ("bytes", "str", "enum"):
                raise SystemExit(
                    f"{path}: {entry['name']}.{field.name}: tipo '{field.type}' desconhecido"
                )

        msg_id = int(entry["id"])
        previous = seen_ids.get(msg_id)
        if previous is not None:
            raise SystemExit(f"{path}: id duplicado 0x{msg_id:04x}: {previous}/{entry['name']}")
        seen_ids[msg_id] = entry["name"]

        messages.append(Message(name=entry["name"], msg_id=msg_id, fields=fields))

    if not messages:
        raise SystemExit(f"{path}: nenhuma mensagem encontrada")

    return Schema(
        major=int(protocol["version"]["major"]),
        minor=int(protocol["version"]["minor"]),
        sof=int(protocol["framing"]["sof"]),
        max_ctrl_payload=int(protocol["framing"]["max_ctrl_payload"]),
        enums=enums,
        messages=tuple(messages),
    )


def msg_lower(name: str) -> str:
    return name.lower()


# ── C ─────────────────────────────────────────────────────────────────────


def c_struct_field_lines(field: Field) -> list[str]:
    if field.type in SCALAR_C_TYPE:
        return [f"    {SCALAR_C_TYPE[field.type]} {field.name};"]
    if field.type == "bytes":
        return [
            f"    uint8_t {field.name}[{field.max}];",
            f"    uint16_t {field.name}_len;",
        ]
    if field.type == "str":
        return [f"    char {field.name}[{field.max} + 1];"]
    if field.type == "enum":
        return [f"    nbp2_{field.enum}_t {field.name};"]
    raise AssertionError(field.type)


def c_encode_field_lines(field: Field) -> list[str]:
    n = field.name
    if field.type in ("u8", "u16", "u32", "u64"):
        return [f"    NBP2_CBOR_TRY(nbp2_cbor_write_uint(&w, in->{n}));"]
    if field.type == "i8":
        return [f"    NBP2_CBOR_TRY(nbp2_cbor_write_int(&w, in->{n}));"]
    if field.type == "f32":
        return [f"    NBP2_CBOR_TRY(nbp2_cbor_write_float32(&w, in->{n}));"]
    if field.type == "bytes":
        return [
            f"    if (in->{n}_len > sizeof(in->{n})) {{ return NBP2_ERR_PAYLOAD_TOO_LARGE; }}",
            f"    NBP2_CBOR_TRY(nbp2_cbor_write_bytes(&w, in->{n}, in->{n}_len));",
        ]
    if field.type == "str":
        return [
            f"    NBP2_CBOR_TRY(nbp2_cbor_write_text(&w, in->{n}, strnlen(in->{n}, sizeof(in->{n}) - 1u)));",
        ]
    if field.type == "enum":
        return [f"    NBP2_CBOR_TRY(nbp2_cbor_write_uint(&w, (uint64_t)in->{n}));"]
    raise AssertionError(field.type)


def c_decode_field_lines(field: Field, enums: dict[str, tuple[str, ...]]) -> list[str]:
    n = field.name
    if field.type in ("u8", "u16", "u32", "u64"):
        max_value = {"u8": "0xFFu", "u16": "0xFFFFu", "u32": "0xFFFFFFFFu"}.get(field.type)
        lines = ["    NBP2_CBOR_TRY(nbp2_cbor_read_uint(&r, &tmp_u));"]
        if max_value is not None:
            lines.append(f"    if (tmp_u > {max_value}) {{ return NBP2_ERR_CBOR_MALFORMED; }}")
        lines.append(f"    out->{n} = ({SCALAR_C_TYPE[field.type]})tmp_u;")
        return lines
    if field.type == "i8":
        return [
            "    NBP2_CBOR_TRY(nbp2_cbor_read_int(&r, &tmp_i));",
            "    if (tmp_i < -128 || tmp_i > 127) { return NBP2_ERR_CBOR_MALFORMED; }",
            f"    out->{n} = (int8_t)tmp_i;",
        ]
    if field.type == "f32":
        return [f"    NBP2_CBOR_TRY(nbp2_cbor_read_float32(&r, &out->{n}));"]
    if field.type == "bytes":
        return [
            f"    NBP2_CBOR_TRY(nbp2_cbor_read_bytes(&r, out->{n}, sizeof(out->{n}), &out->{n}_len));",
        ]
    if field.type == "str":
        return [
            f"    NBP2_CBOR_TRY(nbp2_cbor_read_text(&r, out->{n}, sizeof(out->{n}) - 1u, &tmp_len));",
            f"    out->{n}[tmp_len] = '\\0';",
        ]
    if field.type == "enum":
        count = len(enums[field.enum])
        return [
            "    NBP2_CBOR_TRY(nbp2_cbor_read_uint(&r, &tmp_u));",
            f"    if (tmp_u >= {count}u) {{ return NBP2_ERR_CBOR_MALFORMED; }}",
            f"    out->{n} = (nbp2_{field.enum}_t)tmp_u;",
        ]
    raise AssertionError(field.type)


def c_message_needs(message: Message, kinds: set[str]) -> bool:
    return any(f.type in kinds for f in message.fields)


def write_c_header(schema: Schema, out_dir: Path) -> None:
    enum_blocks = []
    for enum_name, values in schema.enums.items():
        members = "\n".join(
            f"    NBP2_{enum_name.upper()}_{value} = {i}," for i, value in enumerate(values)
        )
        enum_blocks.append(f"typedef enum {{\n{members}\n}} nbp2_{enum_name}_t;")
    enums_src = "\n\n".join(enum_blocks)

    msg_enum_lines = "\n".join(
        f"    NBP2_MSG_{message.name} = 0x{message.msg_id:04X}u," for message in schema.messages
    )

    struct_blocks = []
    proto_blocks = []
    for message in schema.messages:
        lname = msg_lower(message.name)
        field_lines = "\n".join(
            line for field in message.fields for line in c_struct_field_lines(field)
        )
        if not field_lines:
            field_lines = "    uint8_t _unused;"
        struct_blocks.append(f"typedef struct {{\n{field_lines}\n}} nbp2_msg_{lname}_t;")
        proto_blocks.append(
            f"nbp2_status_t nbp2_encode_{lname}(const nbp2_msg_{lname}_t *in, uint8_t *out,\n"
            f"                                  size_t cap, size_t *out_len);\n"
            f"nbp2_status_t nbp2_decode_{lname}(const uint8_t *data, size_t len,\n"
            f"                                  nbp2_msg_{lname}_t *out);"
        )
    structs_src = "\n\n".join(struct_blocks)
    protos_src = "\n\n".join(proto_blocks)

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
{msg_enum_lines}
}} nbp2_msg_type_t;

typedef enum {{
    NBP2_OK = 0,
    NBP2_ERR_INVALID_ARG,
    NBP2_ERR_BUFFER_TOO_SMALL,
    NBP2_ERR_BAD_SOF,
    NBP2_ERR_BAD_CRC,
    NBP2_ERR_PAYLOAD_TOO_LARGE,
    NBP2_ERR_CBOR_MALFORMED,
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

/* Enums do protocolo (protocol/nbp2.yaml §enums). */

{enums_src}

/* CBOR mínimo: array/uint/int/bytes/text/float32 canônicos (RFC 8949),
 * o bastante para codificar os payloads deste protocolo. Sem mapas, sem
 * comprimento indefinido, sem malloc. */

typedef struct {{
    uint8_t *buf;
    size_t cap;
    size_t pos;
}} nbp2_cbor_writer_t;

typedef struct {{
    const uint8_t *buf;
    size_t len;
    size_t pos;
}} nbp2_cbor_reader_t;

void nbp2_cbor_writer_init(nbp2_cbor_writer_t *w, uint8_t *buf, size_t cap);
nbp2_status_t nbp2_cbor_write_array_header(nbp2_cbor_writer_t *w, uint32_t count);
nbp2_status_t nbp2_cbor_write_uint(nbp2_cbor_writer_t *w, uint64_t value);
nbp2_status_t nbp2_cbor_write_int(nbp2_cbor_writer_t *w, int64_t value);
nbp2_status_t nbp2_cbor_write_bytes(nbp2_cbor_writer_t *w, const uint8_t *data, size_t len);
nbp2_status_t nbp2_cbor_write_text(nbp2_cbor_writer_t *w, const char *data, size_t len);
nbp2_status_t nbp2_cbor_write_float32(nbp2_cbor_writer_t *w, float value);

void nbp2_cbor_reader_init(nbp2_cbor_reader_t *r, const uint8_t *buf, size_t len);
nbp2_status_t nbp2_cbor_read_array_header(nbp2_cbor_reader_t *r, uint32_t *out_count);
nbp2_status_t nbp2_cbor_read_uint(nbp2_cbor_reader_t *r, uint64_t *out_value);
nbp2_status_t nbp2_cbor_read_int(nbp2_cbor_reader_t *r, int64_t *out_value);
nbp2_status_t nbp2_cbor_read_bytes(nbp2_cbor_reader_t *r, uint8_t *out_buf, size_t out_cap,
                                   uint16_t *out_len);
nbp2_status_t nbp2_cbor_read_text(nbp2_cbor_reader_t *r, char *out_buf, size_t out_cap,
                                  uint16_t *out_len);
nbp2_status_t nbp2_cbor_read_float32(nbp2_cbor_reader_t *r, float *out_value);

/* Structs e encode/decode por mensagem (protocol/nbp2.yaml §messages). */

{structs_src}

{protos_src}

#ifdef __cplusplus
}}
#endif

#endif
""",
        encoding="utf-8",
    )


def write_c_source(schema: Schema, out_dir: Path) -> None:
    message_funcs = []
    for message in schema.messages:
        lname = msg_lower(message.name)
        n_fields = len(message.fields)

        encode_body = "\n".join(
            line for field in message.fields for line in c_encode_field_lines(field)
        )
        decode_body = "\n".join(
            line for field in message.fields for line in c_decode_field_lines(field, schema.enums)
        )
        needs_tmp_u = any(f.type in ("u8", "u16", "u32", "u64", "enum") for f in message.fields)
        needs_tmp_i = any(f.type == "i8" for f in message.fields)
        needs_tmp_len = any(f.type == "str" for f in message.fields)

        decode_locals = ["    nbp2_cbor_reader_t r;", "    uint32_t count;"]
        if needs_tmp_u:
            decode_locals.append("    uint64_t tmp_u;")
        if needs_tmp_i:
            decode_locals.append("    int64_t tmp_i;")
        if needs_tmp_len:
            decode_locals.append("    uint16_t tmp_len;")

        message_funcs.append(f"""nbp2_status_t nbp2_encode_{lname}(const nbp2_msg_{lname}_t *in, uint8_t *out,
                                  size_t cap, size_t *out_len)
{{
    nbp2_cbor_writer_t w;

    if (in == NULL || out == NULL || out_len == NULL) {{
        return NBP2_ERR_INVALID_ARG;
    }}

    nbp2_cbor_writer_init(&w, out, cap);
    NBP2_CBOR_TRY(nbp2_cbor_write_array_header(&w, {n_fields}u));
{encode_body}

    *out_len = w.pos;
    return NBP2_OK;
}}

nbp2_status_t nbp2_decode_{lname}(const uint8_t *data, size_t len,
                                  nbp2_msg_{lname}_t *out)
{{
{chr(10).join(decode_locals)}

    if (data == NULL || out == NULL) {{
        return NBP2_ERR_INVALID_ARG;
    }}

    nbp2_cbor_reader_init(&r, data, len);
    NBP2_CBOR_TRY(nbp2_cbor_read_array_header(&r, &count));
    if (count != {n_fields}u) {{
        return NBP2_ERR_CBOR_MALFORMED;
    }}
{decode_body}

    return NBP2_OK;
}}""")

    messages_src = "\n\n".join(message_funcs)

    (out_dir / "nbp2.c").write_text(
        r'''#include "nbp2.h"

#include <string.h>

#define NBP2_CBOR_TRY(expr)                                                   \
    do {                                                                     \
        nbp2_status_t _nbp2_status = (expr);                                 \
        if (_nbp2_status != NBP2_OK) {                                       \
            return _nbp2_status;                                            \
        }                                                                    \
    } while (0)

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

/* ── CBOR mínimo (RFC 8949, forma canônica curta) ────────────────────────── */

void nbp2_cbor_writer_init(nbp2_cbor_writer_t *w, uint8_t *buf, size_t cap)
{
    w->buf = buf;
    w->cap = cap;
    w->pos = 0u;
}

static nbp2_status_t nbp2_cbor_write_header(nbp2_cbor_writer_t *w, uint8_t major,
                                            uint64_t value)
{
    uint8_t major_bits = (uint8_t)(major << 5u);

    if (value < 24u) {
        if (w->pos + 1u > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }
        w->buf[w->pos++] = (uint8_t)(major_bits | (uint8_t)value);
        return NBP2_OK;
    }
    if (value <= 0xFFu) {
        if (w->pos + 2u > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }
        w->buf[w->pos++] = (uint8_t)(major_bits | 24u);
        w->buf[w->pos++] = (uint8_t)value;
        return NBP2_OK;
    }
    if (value <= 0xFFFFu) {
        if (w->pos + 3u > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }
        w->buf[w->pos++] = (uint8_t)(major_bits | 25u);
        w->buf[w->pos++] = (uint8_t)((value >> 8u) & 0xFFu);
        w->buf[w->pos++] = (uint8_t)(value & 0xFFu);
        return NBP2_OK;
    }
    if (value <= 0xFFFFFFFFu) {
        if (w->pos + 5u > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }
        w->buf[w->pos++] = (uint8_t)(major_bits | 26u);
        w->buf[w->pos++] = (uint8_t)((value >> 24u) & 0xFFu);
        w->buf[w->pos++] = (uint8_t)((value >> 16u) & 0xFFu);
        w->buf[w->pos++] = (uint8_t)((value >> 8u) & 0xFFu);
        w->buf[w->pos++] = (uint8_t)(value & 0xFFu);
        return NBP2_OK;
    }

    if (w->pos + 9u > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }
    w->buf[w->pos++] = (uint8_t)(major_bits | 27u);
    for (int shift = 56; shift >= 0; shift -= 8) {
        w->buf[w->pos++] = (uint8_t)((value >> (unsigned)shift) & 0xFFu);
    }
    return NBP2_OK;
}

nbp2_status_t nbp2_cbor_write_array_header(nbp2_cbor_writer_t *w, uint32_t count)
{
    return nbp2_cbor_write_header(w, 4u, count);
}

nbp2_status_t nbp2_cbor_write_uint(nbp2_cbor_writer_t *w, uint64_t value)
{
    return nbp2_cbor_write_header(w, 0u, value);
}

nbp2_status_t nbp2_cbor_write_int(nbp2_cbor_writer_t *w, int64_t value)
{
    if (value >= 0) {
        return nbp2_cbor_write_header(w, 0u, (uint64_t)value);
    }
    return nbp2_cbor_write_header(w, 1u, (uint64_t)(-(value + 1)));
}

static nbp2_status_t nbp2_cbor_write_string(nbp2_cbor_writer_t *w, uint8_t major,
                                            const uint8_t *data, size_t len)
{
    nbp2_status_t status = nbp2_cbor_write_header(w, major, (uint64_t)len);

    if (status != NBP2_OK) { return status; }
    if (w->pos + len > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }
    if (len > 0u) {
        memcpy(&w->buf[w->pos], data, len);
    }
    w->pos += len;
    return NBP2_OK;
}

nbp2_status_t nbp2_cbor_write_bytes(nbp2_cbor_writer_t *w, const uint8_t *data, size_t len)
{
    return nbp2_cbor_write_string(w, 2u, data, len);
}

nbp2_status_t nbp2_cbor_write_text(nbp2_cbor_writer_t *w, const char *data, size_t len)
{
    return nbp2_cbor_write_string(w, 3u, (const uint8_t *)data, len);
}

nbp2_status_t nbp2_cbor_write_float32(nbp2_cbor_writer_t *w, float value)
{
    union {
        float f;
        uint32_t u;
    } bits;

    if (w->pos + 5u > w->cap) { return NBP2_ERR_BUFFER_TOO_SMALL; }

    bits.f = value;
    w->buf[w->pos++] = (uint8_t)((7u << 5u) | 26u);
    w->buf[w->pos++] = (uint8_t)((bits.u >> 24u) & 0xFFu);
    w->buf[w->pos++] = (uint8_t)((bits.u >> 16u) & 0xFFu);
    w->buf[w->pos++] = (uint8_t)((bits.u >> 8u) & 0xFFu);
    w->buf[w->pos++] = (uint8_t)(bits.u & 0xFFu);
    return NBP2_OK;
}

void nbp2_cbor_reader_init(nbp2_cbor_reader_t *r, const uint8_t *buf, size_t len)
{
    r->buf = buf;
    r->len = len;
    r->pos = 0u;
}

static nbp2_status_t nbp2_cbor_read_header(nbp2_cbor_reader_t *r, uint8_t expected_major,
                                           uint64_t *out_value)
{
    uint8_t first;
    uint8_t major;
    uint8_t info;

    if (r->pos >= r->len) { return NBP2_ERR_CBOR_MALFORMED; }

    first = r->buf[r->pos];
    major = (uint8_t)(first >> 5u);
    info = (uint8_t)(first & 0x1Fu);
    if (major != expected_major) { return NBP2_ERR_CBOR_MALFORMED; }
    r->pos += 1u;

    if (info < 24u) {
        *out_value = info;
        return NBP2_OK;
    }
    if (info == 24u) {
        if (r->pos + 1u > r->len) { return NBP2_ERR_CBOR_MALFORMED; }
        *out_value = r->buf[r->pos];
        r->pos += 1u;
        return NBP2_OK;
    }
    if (info == 25u) {
        if (r->pos + 2u > r->len) { return NBP2_ERR_CBOR_MALFORMED; }
        *out_value = ((uint64_t)r->buf[r->pos] << 8u) | (uint64_t)r->buf[r->pos + 1u];
        r->pos += 2u;
        return NBP2_OK;
    }
    if (info == 26u) {
        if (r->pos + 4u > r->len) { return NBP2_ERR_CBOR_MALFORMED; }
        *out_value = ((uint64_t)r->buf[r->pos] << 24u) | ((uint64_t)r->buf[r->pos + 1u] << 16u) |
                     ((uint64_t)r->buf[r->pos + 2u] << 8u) | (uint64_t)r->buf[r->pos + 3u];
        r->pos += 4u;
        return NBP2_OK;
    }
    if (info == 27u) {
        uint64_t value = 0u;
        int shift;

        if (r->pos + 8u > r->len) { return NBP2_ERR_CBOR_MALFORMED; }
        for (shift = 0; shift < 8; ++shift) {
            value = (value << 8u) | (uint64_t)r->buf[r->pos + (size_t)shift];
        }
        r->pos += 8u;
        *out_value = value;
        return NBP2_OK;
    }

    return NBP2_ERR_CBOR_MALFORMED;
}

nbp2_status_t nbp2_cbor_read_array_header(nbp2_cbor_reader_t *r, uint32_t *out_count)
{
    uint64_t value;
    nbp2_status_t status = nbp2_cbor_read_header(r, 4u, &value);

    if (status != NBP2_OK) { return status; }
    if (value > 0xFFFFFFFFu) { return NBP2_ERR_CBOR_MALFORMED; }
    *out_count = (uint32_t)value;
    return NBP2_OK;
}

nbp2_status_t nbp2_cbor_read_uint(nbp2_cbor_reader_t *r, uint64_t *out_value)
{
    return nbp2_cbor_read_header(r, 0u, out_value);
}

nbp2_status_t nbp2_cbor_read_int(nbp2_cbor_reader_t *r, int64_t *out_value)
{
    uint8_t major;
    uint64_t magnitude;
    nbp2_status_t status;

    if (r->pos >= r->len) { return NBP2_ERR_CBOR_MALFORMED; }
    major = (uint8_t)(r->buf[r->pos] >> 5u);

    if (major == 0u) {
        status = nbp2_cbor_read_header(r, 0u, &magnitude);
        if (status != NBP2_OK) { return status; }
        if (magnitude > (uint64_t)INT64_MAX) { return NBP2_ERR_CBOR_MALFORMED; }
        *out_value = (int64_t)magnitude;
        return NBP2_OK;
    }
    if (major == 1u) {
        status = nbp2_cbor_read_header(r, 1u, &magnitude);
        if (status != NBP2_OK) { return status; }
        if (magnitude > (uint64_t)INT64_MAX) { return NBP2_ERR_CBOR_MALFORMED; }
        *out_value = -1 - (int64_t)magnitude;
        return NBP2_OK;
    }
    return NBP2_ERR_CBOR_MALFORMED;
}

static nbp2_status_t nbp2_cbor_read_string(nbp2_cbor_reader_t *r, uint8_t major,
                                           uint8_t *out_buf, size_t out_cap, uint16_t *out_len)
{
    uint64_t length;
    nbp2_status_t status = nbp2_cbor_read_header(r, major, &length);

    if (status != NBP2_OK) { return status; }
    if (length > out_cap) { return NBP2_ERR_PAYLOAD_TOO_LARGE; }
    if (r->pos + length > r->len) { return NBP2_ERR_CBOR_MALFORMED; }
    if (length > 0u) {
        memcpy(out_buf, &r->buf[r->pos], (size_t)length);
    }
    r->pos += (size_t)length;
    *out_len = (uint16_t)length;
    return NBP2_OK;
}

nbp2_status_t nbp2_cbor_read_bytes(nbp2_cbor_reader_t *r, uint8_t *out_buf, size_t out_cap,
                                   uint16_t *out_len)
{
    return nbp2_cbor_read_string(r, 2u, out_buf, out_cap, out_len);
}

nbp2_status_t nbp2_cbor_read_text(nbp2_cbor_reader_t *r, char *out_buf, size_t out_cap,
                                  uint16_t *out_len)
{
    return nbp2_cbor_read_string(r, 3u, (uint8_t *)out_buf, out_cap, out_len);
}

nbp2_status_t nbp2_cbor_read_float32(nbp2_cbor_reader_t *r, float *out_value)
{
    union {
        float f;
        uint32_t u;
    } bits;

    if (r->pos + 5u > r->len) { return NBP2_ERR_CBOR_MALFORMED; }
    if (r->buf[r->pos] != ((7u << 5u) | 26u)) { return NBP2_ERR_CBOR_MALFORMED; }

    bits.u = ((uint32_t)r->buf[r->pos + 1u] << 24u) | ((uint32_t)r->buf[r->pos + 2u] << 16u) |
             ((uint32_t)r->buf[r->pos + 3u] << 8u) | (uint32_t)r->buf[r->pos + 4u];
    r->pos += 5u;
    *out_value = bits.f;
    return NBP2_OK;
}

'''
        + messages_src
        + "\n",
        encoding="utf-8",
    )


# ── Python ────────────────────────────────────────────────────────────────


def py_class_name(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


def py_safe_name(name: str) -> str:
    return f"{name}_" if keyword.iskeyword(name) else name


def py_field_type(field: Field) -> str:
    if field.type in ("u8", "u16", "u32", "u64", "i8"):
        return "int"
    if field.type == "f32":
        return "float"
    if field.type == "bytes":
        return "bytes"
    if field.type == "str":
        return "str"
    if field.type == "enum":
        return py_class_name(field.enum)
    raise AssertionError(field.type)


def py_encode_field_lines(field: Field) -> list[str]:
    n = py_safe_name(field.name)
    if field.type in ("u8", "u16", "u32", "u64"):
        return [f"    cbor_write_uint(buf, msg.{n})"]
    if field.type == "i8":
        return [f"    cbor_write_int(buf, msg.{n})"]
    if field.type == "f32":
        return [f"    cbor_write_float32(buf, msg.{n})"]
    if field.type == "bytes":
        return [
            f"    if len(msg.{n}) > {field.max}:",
            f'        raise ValueError("{n} excede o maximo de {field.max} bytes")',
            f"    cbor_write_bytes(buf, msg.{n})",
        ]
    if field.type == "str":
        return [
            f"    _{n}_encoded = msg.{n}.encode(\"utf-8\")",
            f"    if len(_{n}_encoded) > {field.max}:",
            f'        raise ValueError("{n} excede o maximo de {field.max} bytes")',
            f"    cbor_write_text(buf, msg.{n})",
        ]
    if field.type == "enum":
        return [f"    cbor_write_uint(buf, int(msg.{n}))"]
    raise AssertionError(field.type)


def py_decode_field_lines(field: Field) -> list[str]:
    n = py_safe_name(field.name)
    if field.type in ("u8", "u16", "u32", "u64"):
        return [f"    {n} = reader.read_uint()"]
    if field.type == "i8":
        return [f"    {n} = reader.read_int()"]
    if field.type == "f32":
        return [f"    {n} = reader.read_float32()"]
    if field.type == "bytes":
        return [f"    {n} = reader.read_bytes()"]
    if field.type == "str":
        return [f"    {n} = reader.read_text()"]
    if field.type == "enum":
        return [f"    {n} = {py_class_name(field.enum)}(reader.read_uint())"]
    raise AssertionError(field.type)


def write_python(schema: Schema, out_dir: Path) -> None:
    constants = "\n".join(
        f"MSG_{message.name} = 0x{message.msg_id:04X}" for message in schema.messages
    )

    enum_blocks = []
    for enum_name, values in schema.enums.items():
        members = "\n".join(f"    {value} = {i}" for i, value in enumerate(values))
        enum_blocks.append(f"class {py_class_name(enum_name)}(IntEnum):\n{members}")
    enums_src = "\n\n\n".join(enum_blocks)

    message_blocks = []
    for message in schema.messages:
        lname = msg_lower(message.name)
        cls_name = py_class_name(message.name)

        if message.fields:
            field_decls = "\n".join(
                f"    {py_safe_name(f.name)}: {py_field_type(f)}" for f in message.fields
            )
        else:
            field_decls = "    pass"

        encode_lines = "\n".join(
            line for field in message.fields for line in py_encode_field_lines(field)
        )
        decode_lines = "\n".join(
            line for field in message.fields for line in py_decode_field_lines(field)
        )
        ctor_args = ", ".join(py_safe_name(f.name) for f in message.fields)

        message_blocks.append(
            f'''@dataclass(frozen=True)
class {cls_name}:
{field_decls}


def encode_{lname}(msg: {cls_name}) -> bytes:
    buf = bytearray()
    cbor_write_array_header(buf, {len(message.fields)})
{encode_lines if encode_lines else "    pass"}
    return bytes(buf)


def decode_{lname}(data: bytes) -> {cls_name}:
    reader = CborReader(data)
    count = reader.read_array_header()
    if count != {len(message.fields)}:
        raise ValueError("{message.name.lower()}: numero de campos invalido")
{decode_lines if decode_lines else "    pass"}
    return {cls_name}({ctor_args})'''
        )
    messages_src = "\n\n\n".join(message_blocks)

    (out_dir / "nbp2.py").write_text(
        f'''"""Generated NBP/2 framing + CBOR payload helpers. Do not edit by hand."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
import struct
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


# ── CBOR minimo (RFC 8949, forma canonica curta) ─────────────────────────

def _write_header(buf: bytearray, major: int, value: int) -> None:
    top = major << 5
    if value < 24:
        buf.append(top | value)
    elif value <= 0xFF:
        buf.append(top | 24)
        buf.append(value)
    elif value <= 0xFFFF:
        buf.append(top | 25)
        buf += value.to_bytes(2, "big")
    elif value <= 0xFFFFFFFF:
        buf.append(top | 26)
        buf += value.to_bytes(4, "big")
    else:
        buf.append(top | 27)
        buf += value.to_bytes(8, "big")


def cbor_write_array_header(buf: bytearray, count: int) -> None:
    _write_header(buf, 4, count)


def cbor_write_uint(buf: bytearray, value: int) -> None:
    _write_header(buf, 0, value)


def cbor_write_int(buf: bytearray, value: int) -> None:
    if value >= 0:
        _write_header(buf, 0, value)
    else:
        _write_header(buf, 1, -value - 1)


def cbor_write_bytes(buf: bytearray, data: bytes) -> None:
    _write_header(buf, 2, len(data))
    buf += data


def cbor_write_text(buf: bytearray, text: str) -> None:
    encoded = text.encode("utf-8")
    _write_header(buf, 3, len(encoded))
    buf += encoded


def cbor_write_float32(buf: bytearray, value: float) -> None:
    buf.append(0xFA)
    buf += struct.pack(">f", value)


class CborReader:
    def __init__(self, data: bytes) -> None:
        self.data = data
        self.pos = 0

    def _read_header(self, expected_major: int) -> int:
        if self.pos >= len(self.data):
            raise ValueError("cbor truncated")
        first = self.data[self.pos]
        major = first >> 5
        info = first & 0x1F
        if major != expected_major:
            raise ValueError("cbor unexpected major type")
        self.pos += 1
        if info < 24:
            return info
        if info == 24:
            value = self.data[self.pos]
            self.pos += 1
            return value
        if info == 25:
            value = int.from_bytes(self.data[self.pos : self.pos + 2], "big")
            self.pos += 2
            return value
        if info == 26:
            value = int.from_bytes(self.data[self.pos : self.pos + 4], "big")
            self.pos += 4
            return value
        if info == 27:
            value = int.from_bytes(self.data[self.pos : self.pos + 8], "big")
            self.pos += 8
            return value
        raise ValueError("cbor unsupported additional info")

    def read_array_header(self) -> int:
        return self._read_header(4)

    def read_uint(self) -> int:
        return self._read_header(0)

    def read_int(self) -> int:
        if self.pos >= len(self.data):
            raise ValueError("cbor truncated")
        major = self.data[self.pos] >> 5
        if major == 0:
            return self._read_header(0)
        if major == 1:
            return -1 - self._read_header(1)
        raise ValueError("cbor unexpected major type for int")

    def _read_string(self, major: int) -> bytes:
        length = self._read_header(major)
        data = self.data[self.pos : self.pos + length]
        if len(data) != length:
            raise ValueError("cbor truncated string")
        self.pos += length
        return data

    def read_bytes(self) -> bytes:
        return self._read_string(2)

    def read_text(self) -> str:
        return self._read_string(3).decode("utf-8")

    def read_float32(self) -> float:
        if self.pos >= len(self.data) or self.data[self.pos] != 0xFA:
            raise ValueError("cbor expected float32")
        self.pos += 1
        value = struct.unpack(">f", self.data[self.pos : self.pos + 4])[0]
        self.pos += 4
        return value


# ── Enums do protocolo ────────────────────────────────────────────────────

{enums_src}


# ── Mensagens (structs + encode/decode CBOR) ─────────────────────────────

{messages_src}
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
    write_c_source(schema, c_dir)
    write_python(schema, py_dir)
    print(f"nbp2-codegen: {len(schema.messages)} mensagens geradas em {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
