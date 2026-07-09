"""SQLite store enxuto para conversas e turnos do NoiseBot 2."""

from __future__ import annotations

import sqlite3
import threading
from pathlib import Path
from typing import Any

SCHEMA_VERSION = 1


class ConversationStoreError(RuntimeError):
    """Erro de uso seguro do store persistente."""


class ConversationStore:
    """Store SQLite com transacoes curtas e explicitas."""

    def __init__(self, path: str | Path) -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self._lock = threading.Lock()
        self._initialize()

    @property
    def schema_version(self) -> int:
        with self._connect() as conn:
            row = conn.execute("PRAGMA user_version").fetchone()
        return int(row[0]) if row else 0

    def create_conversation(
        self,
        *,
        user_id: str,
        title: str,
        conversation_id: str,
    ) -> dict[str, Any]:
        with self._tx() as conn:
            conn.execute(
                """
                INSERT INTO conversations (id, user_id, title)
                VALUES (?, ?, ?)
                """,
                (conversation_id, user_id, title),
            )
            row = conn.execute(
                "SELECT id, user_id, title, created_at_ms, updated_at_ms FROM conversations WHERE id = ?",
                (conversation_id,),
            ).fetchone()
        return self._conversation_row_to_dict(row)

    def get_conversation(self, conversation_id: str, *, user_id: str) -> dict[str, Any]:
        with self._connect() as conn:
            row = conn.execute(
                """
                SELECT id, user_id, title, created_at_ms, updated_at_ms
                FROM conversations
                WHERE id = ? AND user_id = ?
                """,
                (conversation_id, user_id),
            ).fetchone()
        if row is None:
            raise ConversationStoreError("conversa nao encontrada")
        return self._conversation_row_to_dict(row)

    def update_conversation(
        self,
        conversation_id: str,
        *,
        user_id: str,
        title: str,
    ) -> dict[str, Any]:
        with self._tx() as conn:
            result = conn.execute(
                """
                UPDATE conversations
                SET title = ?, updated_at_ms = _nb_now_ms()
                WHERE id = ? AND user_id = ?
                """,
                (title, conversation_id, user_id),
            )
            if result.rowcount == 0:
                raise ConversationStoreError("conversa nao encontrada")
            row = conn.execute(
                "SELECT id, user_id, title, created_at_ms, updated_at_ms FROM conversations WHERE id = ?",
                (conversation_id,),
            ).fetchone()
        return self._conversation_row_to_dict(row)

    def delete_conversation(self, conversation_id: str, *, user_id: str) -> bool:
        with self._tx() as conn:
            conn.execute(
                "DELETE FROM active_conversations WHERE user_id = ? AND conversation_id = ?",
                (user_id, conversation_id),
            )
            result = conn.execute(
                "DELETE FROM conversations WHERE id = ? AND user_id = ?",
                (conversation_id, user_id),
            )
        return result.rowcount > 0

    def set_active_conversation(self, *, user_id: str, conversation_id: str) -> None:
        with self._tx() as conn:
            exists = conn.execute(
                "SELECT 1 FROM conversations WHERE id = ? AND user_id = ?",
                (conversation_id, user_id),
            ).fetchone()
            if exists is None:
                raise ConversationStoreError("conversa ativa inexistente")
            conn.execute(
                """
                INSERT INTO active_conversations (user_id, conversation_id)
                VALUES (?, ?)
                ON CONFLICT(user_id)
                DO UPDATE SET conversation_id = excluded.conversation_id
                """,
                (user_id, conversation_id),
            )

    def get_active_conversation(self, *, user_id: str) -> dict[str, Any] | None:
        with self._connect() as conn:
            row = conn.execute(
                """
                SELECT c.id, c.user_id, c.title, c.created_at_ms, c.updated_at_ms
                FROM active_conversations ac
                JOIN conversations c ON c.id = ac.conversation_id
                WHERE ac.user_id = ?
                """,
                (user_id,),
            ).fetchone()
        if row is None:
            return None
        return self._conversation_row_to_dict(row)

    def begin_turn(
        self,
        *,
        conversation_id: str,
        turn_id: int,
        request_id: str,
        user_id: str,
        transcript: str,
    ) -> dict[str, Any]:
        with self._tx() as conn:
            existing = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE request_id = ?
                """,
                (request_id,),
            ).fetchone()
            if existing is not None:
                return self._turn_row_to_dict(existing)

            convo = conn.execute(
                "SELECT 1 FROM conversations WHERE id = ? AND user_id = ?",
                (conversation_id, user_id),
            ).fetchone()
            if convo is None:
                raise ConversationStoreError("conversa nao encontrada")

            conn.execute(
                """
                INSERT INTO turns (id, conversation_id, request_id, user_id, transcript, status)
                VALUES (?, ?, ?, ?, ?, 'pending')
                """,
                (turn_id, conversation_id, request_id, user_id, transcript),
            )
            conn.execute(
                """
                INSERT INTO messages (conversation_id, turn_id, role, content)
                VALUES (?, ?, 'user', ?)
                """,
                (conversation_id, turn_id, transcript),
            )
            row = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE id = ?
                """,
                (turn_id,),
            ).fetchone()
        return self._turn_row_to_dict(row)

    def complete_turn(
        self,
        turn_id: int,
        *,
        reply: str,
        route: str,
    ) -> dict[str, Any]:
        with self._tx() as conn:
            row = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE id = ?
                """,
                (turn_id,),
            ).fetchone()
            if row is None:
                raise ConversationStoreError("turno nao encontrado")
            if row[6] == "completed":
                return self._turn_row_to_dict(row)

            conn.execute(
                """
                UPDATE turns
                SET reply = ?, route = ?, status = 'completed', updated_at_ms = _nb_now_ms()
                WHERE id = ?
                """,
                (reply, route, turn_id),
            )
            conn.execute(
                """
                INSERT INTO messages (conversation_id, turn_id, role, content)
                VALUES (?, ?, 'assistant', ?)
                """,
                (row[1], turn_id, reply),
            )
            updated = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE id = ?
                """,
                (turn_id,),
            ).fetchone()
        return self._turn_row_to_dict(updated)

    def finish_turn_with_error(self, turn_id: int, *, error_code: str) -> dict[str, Any]:
        with self._tx() as conn:
            row = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE id = ?
                """,
                (turn_id,),
            ).fetchone()
            if row is None:
                raise ConversationStoreError("turno nao encontrado")
            conn.execute(
                """
                UPDATE turns
                SET status = 'error', error_code = ?, updated_at_ms = _nb_now_ms()
                WHERE id = ?
                """,
                (error_code, turn_id),
            )
            updated = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE id = ?
                """,
                (turn_id,),
            ).fetchone()
        return self._turn_row_to_dict(updated)

    def get_turn(self, turn_id: int) -> dict[str, Any]:
        with self._connect() as conn:
            row = conn.execute(
                """
                SELECT id, conversation_id, request_id, user_id, transcript, reply,
                       status, error_code, created_at_ms, updated_at_ms
                FROM turns
                WHERE id = ?
                """,
                (turn_id,),
            ).fetchone()
        if row is None:
            raise ConversationStoreError("turno nao encontrado")
        return self._turn_row_to_dict(row)

    def list_messages(
        self,
        *,
        conversation_id: str,
        limit: int,
        before_message_id: int | None = None,
    ) -> list[dict[str, Any]]:
        query = """
            SELECT id, conversation_id, turn_id, role, content, created_at_ms
            FROM messages
            WHERE conversation_id = ?
        """
        params: list[Any] = [conversation_id]
        if before_message_id is not None:
            query += " AND id < ?"
            params.append(before_message_id)
        query += " ORDER BY id DESC LIMIT ?"
        params.append(limit)

        with self._connect() as conn:
            rows = conn.execute(query, params).fetchall()
        return [self._message_row_to_dict(row) for row in rows]

    def recover_stale_pending(self, *, older_than_ms: int, now_ms: int) -> int:
        threshold_ms = now_ms - older_than_ms
        with self._tx() as conn:
            result = conn.execute(
                """
                UPDATE turns
                SET status = 'interrupted',
                    error_code = 'server_restart',
                    updated_at_ms = ?
                WHERE status = 'pending' AND updated_at_ms < ?
                """,
                (now_ms, threshold_ms),
            )
        return result.rowcount

    def _initialize(self) -> None:
        with self._connect() as conn:
            current = int(conn.execute("PRAGMA user_version").fetchone()[0])
            if current > SCHEMA_VERSION:
                raise ConversationStoreError("schema do banco e mais novo que o suportado")
            if current == 0:
                self._create_schema(conn)
                conn.execute(f"PRAGMA user_version = {SCHEMA_VERSION}")

    def _create_schema(self, conn: sqlite3.Connection) -> None:
        conn.executescript(
            """
            CREATE TABLE IF NOT EXISTS conversations (
                id TEXT PRIMARY KEY,
                user_id TEXT NOT NULL,
                title TEXT NOT NULL,
                created_at_ms INTEGER NOT NULL DEFAULT (_nb_now_ms()),
                updated_at_ms INTEGER NOT NULL DEFAULT (_nb_now_ms())
            );

            CREATE TABLE IF NOT EXISTS active_conversations (
                user_id TEXT PRIMARY KEY,
                conversation_id TEXT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE
            );

            CREATE TABLE IF NOT EXISTS turns (
                id INTEGER PRIMARY KEY,
                conversation_id TEXT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
                request_id TEXT NOT NULL UNIQUE,
                user_id TEXT NOT NULL,
                transcript TEXT NOT NULL,
                reply TEXT NOT NULL DEFAULT '',
                route TEXT NOT NULL DEFAULT '',
                status TEXT NOT NULL,
                error_code TEXT NOT NULL DEFAULT '',
                created_at_ms INTEGER NOT NULL DEFAULT (_nb_now_ms()),
                updated_at_ms INTEGER NOT NULL DEFAULT (_nb_now_ms())
            );

            CREATE TABLE IF NOT EXISTS messages (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                conversation_id TEXT NOT NULL REFERENCES conversations(id) ON DELETE CASCADE,
                turn_id INTEGER NOT NULL REFERENCES turns(id) ON DELETE CASCADE,
                role TEXT NOT NULL,
                content TEXT NOT NULL,
                created_at_ms INTEGER NOT NULL DEFAULT (_nb_now_ms())
            );
            """
        )

    def _connect(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.path)
        conn.row_factory = sqlite3.Row
        conn.execute("PRAGMA foreign_keys = ON")
        conn.create_function("_nb_now_ms", 0, lambda: 0)
        return conn

    def _tx(self):
        return _ConversationStoreTx(self)

    @staticmethod
    def _conversation_row_to_dict(row: sqlite3.Row | tuple[Any, ...]) -> dict[str, Any]:
        return {
            "id": row[0],
            "user_id": row[1],
            "title": row[2],
            "created_at_ms": row[3],
            "updated_at_ms": row[4],
        }

    @staticmethod
    def _turn_row_to_dict(row: sqlite3.Row | tuple[Any, ...]) -> dict[str, Any]:
        return {
            "id": row[0],
            "conversation_id": row[1],
            "request_id": row[2],
            "user_id": row[3],
            "transcript": row[4],
            "reply": row[5] or "",
            "status": row[6],
            "error_code": row[7] or "",
            "created_at_ms": row[8],
            "updated_at_ms": row[9],
        }

    @staticmethod
    def _message_row_to_dict(row: sqlite3.Row | tuple[Any, ...]) -> dict[str, Any]:
        return {
            "id": row[0],
            "conversation_id": row[1],
            "turn_id": row[2],
            "role": row[3],
            "content": row[4],
            "created_at_ms": row[5],
        }


class _ConversationStoreTx:
    def __init__(self, store: ConversationStore) -> None:
        self._store = store
        self._conn: sqlite3.Connection | None = None

    def __enter__(self) -> sqlite3.Connection:
        self._store._lock.acquire()
        self._conn = self._store._connect()
        self._conn.execute("BEGIN IMMEDIATE")
        return self._conn

    def __exit__(self, exc_type, exc, _tb) -> None:
        assert self._conn is not None
        try:
            if exc_type is None:
                self._conn.commit()
            else:
                self._conn.rollback()
        finally:
            self._conn.close()
            self._store._lock.release()


__all__ = ["ConversationStore", "ConversationStoreError", "SCHEMA_VERSION"]
