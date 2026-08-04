"""SQLite 存储：用户与云端历史记录。

无需额外依赖（标准库 sqlite3），数据库文件为项目根目录的 aimusic.db。
"""
from __future__ import annotations

import json
import sqlite3
from contextlib import closing
from pathlib import Path
from typing import Optional

from .schemas import HistoryItemIn, HistoryItemOut, NoteOut

# 数据库文件（与 output/ 同级的项目根目录）
DB_PATH = Path(__file__).resolve().parent.parent / "aimusic.db"


def get_conn() -> sqlite3.Connection:
    conn = sqlite3.connect(DB_PATH)
    conn.row_factory = sqlite3.Row
    return conn


def init_db() -> None:
    """建表（幂等）。应用启动时调用。"""
    with closing(get_conn()) as conn:
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS users (
                openid     TEXT PRIMARY KEY,
                token      TEXT NOT NULL,
                created_at TEXT NOT NULL DEFAULT (datetime('now'))
            )
            """
        )
        conn.execute(
            """
            CREATE TABLE IF NOT EXISTS history (
                id             INTEGER PRIMARY KEY AUTOINCREMENT,
                openid         TEXT NOT NULL,
                title          TEXT NOT NULL DEFAULT 'Untitled',
                key            TEXT NOT NULL DEFAULT 'C major',
                tempo          INTEGER NOT NULL DEFAULT 100,
                time_signature TEXT NOT NULL DEFAULT '4/4',
                composer_note  TEXT,
                notes          TEXT NOT NULL DEFAULT '[]',
                midi_url       TEXT,
                created_at     TEXT NOT NULL DEFAULT (datetime('now')),
                FOREIGN KEY(openid) REFERENCES users(openid)
            )
            """
        )
        conn.execute("CREATE INDEX IF NOT EXISTS idx_history_openid ON history(openid)")
        conn.commit()


# ========================= 用户 =========================
def upsert_user(openid: str, token: str) -> None:
    with closing(get_conn()) as conn:
        conn.execute(
            "INSERT OR REPLACE INTO users (openid, token) VALUES (?, ?)",
            (openid, token),
        )
        conn.commit()


def get_user_by_token(token: str) -> Optional[dict]:
    with closing(get_conn()) as conn:
        row = conn.execute("SELECT * FROM users WHERE token = ?", (token,)).fetchone()
    return dict(row) if row else None


# ========================= 历史记录 =========================
def _row_to_item(row: sqlite3.Row) -> HistoryItemOut:
    notes = [NoteOut(**n) for n in json.loads(row["notes"] or "[]")]
    return HistoryItemOut(
        id=row["id"],
        title=row["title"],
        key=row["key"],
        tempo=row["tempo"],
        time_signature=row["time_signature"],
        composer_note=row["composer_note"],
        notes=notes,
        midi_url=row["midi_url"],
        created_at=row["created_at"],
    )


def add_history(openid: str, item: HistoryItemIn) -> HistoryItemOut:
    notes_json = json.dumps([n.model_dump() for n in item.notes], ensure_ascii=False)
    with closing(get_conn()) as conn:
        cur = conn.execute(
            """
            INSERT INTO history
                (openid, title, key, tempo, time_signature, composer_note, notes, midi_url)
            VALUES (?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                openid,
                item.title or "Untitled",
                item.key or "C major",
                item.tempo or 100,
                item.time_signature or "4/4",
                item.composer_note,
                notes_json,
                item.midi_url,
            ),
        )
        conn.commit()
        row = conn.execute("SELECT * FROM history WHERE id = ?", (cur.lastrowid,)).fetchone()
    return _row_to_item(row)


def list_history(openid: str) -> list[HistoryItemOut]:
    with closing(get_conn()) as conn:
        rows = conn.execute(
            "SELECT * FROM history WHERE openid = ? ORDER BY id DESC", (openid,)
        ).fetchall()
    return [_row_to_item(r) for r in rows]


def delete_history(openid: str, history_id: int) -> bool:
    with closing(get_conn()) as conn:
        cur = conn.execute(
            "DELETE FROM history WHERE id = ? AND openid = ?", (history_id, openid)
        )
        conn.commit()
    return cur.rowcount > 0
