--
-- PayPal real-money settlement ledger (Commerce Rail A).
--
-- Written by the bnetserver webhook receiver (status transitions on verified PayPal events) and
-- read by the worldserver PaymentMgr, which grants SETTLED rows and flips them to DELIVERED in the
-- same transaction so a webhook retry can never double-grant. Lives in the AUTH (login) DB because
-- BOTH processes hold an auth-DB connection.
--
CREATE TABLE IF NOT EXISTS `paypal_settlement` (
  `orderId`     VARCHAR(32)     NOT NULL,
  `captureId`   VARCHAR(32)     NULL,
  `accountId`   INT UNSIGNED    NOT NULL,
  `productId`   BIGINT UNSIGNED NOT NULL,
  `amount`      VARCHAR(16)     NOT NULL,
  `currency`    VARCHAR(4)      NOT NULL,
  `status`      ENUM('CREATED','APPROVED','SETTLED','DELIVERED','DENIED','REFUNDED') NOT NULL DEFAULT 'CREATED',
  `createdAt`   TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `updatedAt`   TIMESTAMP       NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (`orderId`),
  KEY `idx_status` (`status`),
  KEY `idx_account` (`accountId`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
