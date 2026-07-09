from __future__ import annotations

import sqlite3
from pathlib import Path

import pytest

from noisebot2.stores import ConversationStore, ConversationStoreError


def _store(tmp_path: Path) -> ConversationStore:
    return ConversationStore(tmp_path / "conversations.sqlite3")


def _conversation(store: ConversationStore, *, user_id: str = "owner") -> dict:
    return store.create_conversation(
        user_id=user_id,
        title="Sessao principal",
        conversation_id=f"{user_id}-main",
    )


def test_store_creates_schema_and_survives_restart(tmp_path: Path) -> None:
    path = tmp_path / "conversations.sqlite3"
    first = ConversationStore(path)
    assert first.schema_version == 1

    second = ConversationStore(path)
    assert second.schema_version == 1


def test_store_rejects_newer_schema_without_modifying_it(tmp_path: Path) -> None:
    path = tmp_path / "conversations.sqlite3"
    store = ConversationStore(path)
    assert store.schema_version == 1

    with sqlite3.connect(path) as conn:
        conn.execute("PRAGMA user_version = 99")

    with pytest.raises(ConversationStoreError, match="mais novo"):
        ConversationStore(path)


def test_store_tracks_active_conversation_per_user(tmp_path: Path) -> None:
    store = _store(tmp_path)
    owner = _conversation(store, user_id="owner")
    guest = _conversation(store, user_id="guest")

    store.set_active_conversation(user_id="owner", conversation_id=owner["id"])
    store.set_active_conversation(user_id="guest", conversation_id=guest["id"])

    assert store.get_active_conversation(user_id="owner")["id"] == owner["id"]
    assert store.get_active_conversation(user_id="guest")["id"] == guest["id"]

    assert store.delete_conversation(owner["id"], user_id="owner") is True
    assert store.get_active_conversation(user_id="owner") is None


def test_store_begin_turn_is_idempotent_and_lists_messages(tmp_path: Path) -> None:
    store = _store(tmp_path)
    conversation = _conversation(store)

    first = store.begin_turn(
        conversation_id=conversation["id"],
        turn_id=7,
        request_id="req-7",
        user_id="owner",
        transcript="ola",
    )
    repeated = store.begin_turn(
        conversation_id=conversation["id"],
        turn_id=999,
        request_id="req-7",
        user_id="owner",
        transcript="nao deve duplicar",
    )
    completed = store.complete_turn(7, reply="Oi!", route="llm")

    assert first["id"] == 7
    assert repeated["id"] == 7
    assert completed["status"] == "completed"

    messages = store.list_messages(conversation_id=conversation["id"], limit=10)
    assert len(messages) == 2
    assert messages[0]["role"] == "assistant"
    assert messages[0]["content"] == "Oi!"
    assert messages[1]["role"] == "user"
    assert messages[1]["content"] == "ola"


def test_store_recovers_stale_pending_turns(tmp_path: Path) -> None:
    store = _store(tmp_path)
    conversation = _conversation(store)

    store.begin_turn(
        conversation_id=conversation["id"],
        turn_id=10,
        request_id="req-old",
        user_id="owner",
        transcript="turno velho",
    )
    store.begin_turn(
        conversation_id=conversation["id"],
        turn_id=11,
        request_id="req-new",
        user_id="owner",
        transcript="turno novo",
    )

    with sqlite3.connect(store.path) as conn:
        conn.execute("UPDATE turns SET updated_at_ms = 1000 WHERE id = 10")
        conn.execute("UPDATE turns SET updated_at_ms = 9000 WHERE id = 11")

    assert store.recover_stale_pending(older_than_ms=2000, now_ms=10_000) == 1
    assert store.get_turn(10)["status"] == "interrupted"
    assert store.get_turn(10)["error_code"] == "server_restart"
    assert store.get_turn(11)["status"] == "pending"
