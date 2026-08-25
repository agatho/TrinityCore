-- Migration: 0013_craft_orders
-- Date:    2026-06-15
-- Purpose: Bot-to-bot CRAFT-ORDER BOARD + escrow plumbing (#4B-2(a) part 1 of
--          docs/LIVING_SERVER_PLAN_20260614.md). Lets a requester bot post an
--          order for an item it can't make itself (escrowing the payment up
--          front by debiting its gold), a crafter bot that KNOWS the recipe
--          claim + fulfil it, and the escrow release/refund to settle the
--          transaction exactly once. This closes the profession economy loop:
--          gatherers feed reagents into the AH, crafters turn reagents into
--          finished goods on demand, and gold circulates between bots.
--
--          SECURITY (#4B-2 human-firewall): this is a CLOSED bot-to-bot system.
--          requester_low AND crafter_low are re-verified as CURRENT fleet bots
--          (Services::Registry()) at every state transition by CraftOrderBoard.
--          There is NO GM command / whisper / human entry point — a real player
--          can neither post, claim, fulfil, nor extract value from an order.
--
--          Columns:
--            id              auto PK; the order id carried by snapshot + intent.
--            requester_low   Player guid-low of the bot that posted the order
--                            and whose gold was escrowed.
--            crafter_low     Player guid-low of the bot that claimed it; NULL
--                            while the order is still Open.
--            spell_id        The craft recipe spell the order wants fulfilled.
--            item_entry      The product item the recipe creates (item_template
--                            entry), carried so the board / snapshot can describe
--                            the order without re-resolving the spell.
--            quantity        How many of the product the requester wants.
--            payment_copper  The escrowed payment (copper). Debited from the
--                            requester at PostOrder; paid to the crafter at
--                            MarkDelivered OR refunded to the requester at
--                            Fail/Cancel — EXACTLY ONCE, never both, never
--                            neither. The `status` column is the single source
--                            of truth that guards which of those two settlements
--                            (if any) has already happened.
--            status          0=Open 1=Claimed 2=Delivered 3=Failed 4=Cancelled.
--            created_at      When the order was posted (escrow taken).
--            claimed_at      When a crafter claimed it; NULL while Open.
--
--          ESCROW INVARIANT (documented here + in CraftOrderBoard.h):
--            payment_copper is removed from the requester's gold EXACTLY ONCE,
--            at the Open transition (PostOrder). It then has exactly one of two
--            terminal fates:
--              * Delivered  -> paid to the crafter (MarkDelivered), or
--              * Failed/Cancelled -> refunded to the requester (FailOrder).
--            A row may be settled only while transitioning OUT of a non-terminal
--            status (Open/Claimed) INTO a terminal one. Terminal rows
--            (Delivered/Failed/Cancelled) are never re-settled, so the gold can
--            never be double-released or double-refunded.
--
-- Reverts: yes (DROP TABLE).
--
-- Retention: finished rows (Delivered/Failed/Cancelled) are pruned by
--            CraftOrderBoard::Tick after a grace window; Open/Claimed rows are
--            reconciled into memory on load.

CREATE TABLE IF NOT EXISTS bot_craft_orders (
    id              BIGINT  UNSIGNED NOT NULL AUTO_INCREMENT PRIMARY KEY,
    requester_low   BIGINT  UNSIGNED NOT NULL,
    crafter_low     BIGINT  UNSIGNED NULL DEFAULT NULL,
    spell_id        INT     UNSIGNED NOT NULL DEFAULT 0,
    item_entry      INT     UNSIGNED NOT NULL DEFAULT 0,
    quantity        INT     UNSIGNED NOT NULL DEFAULT 1,
    payment_copper  BIGINT  UNSIGNED NOT NULL DEFAULT 0,
    status          TINYINT UNSIGNED NOT NULL DEFAULT 0,
    created_at      DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    claimed_at      DATETIME NULL DEFAULT NULL,
    KEY idx_status (status)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- Record this migration as applied (mirrors the 0011/0012 record-version pattern).
INSERT INTO playerbot_v2_schema_version (version, sha256) VALUES
    (13, REPEAT('0', 64))
ON DUPLICATE KEY UPDATE applied_at = CURRENT_TIMESTAMP;
