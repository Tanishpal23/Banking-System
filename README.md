# Core Banking System

[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![Platform](https://img.shields.io/badge/Platform-Windows%20%7C%20Linux%20%7C%20macOS-blue)](#compilation--running)
[![Compiler](https://img.shields.io/badge/Compiler-GCC%20%7C%20Clang%20%7C%20MSVC-orange)](#compilation--running)
[![Storage](https://img.shields.io/badge/Storage-Persistent%20Disk%20IO-47A248)](#persistence--storage-design)
[![UI](https://img.shields.io/badge/UI-ANSI%20Color%20Console-blueviolet)](#overview--key-features)
[![Security](https://img.shields.io/badge/Security-PIN%20Auth%20%2B%20Audit%20Log-critical)](#security--business-rules)
[![License](https://img.shields.io/badge/License-MIT-green)](LICENSE)
[![GitHub](https://img.shields.io/badge/GitHub-tanishpal23-181717?logo=github)](https://github.com/tanishpal23)

A lightweight, self-contained Core Banking System CLI application written in **C++17**, featuring robust business rule validation, PIN-secured transactions, disk persistence, append-only audit logging, and an interactive, colorized terminal UI.

---

## Table of Contents
- [Overview & Key Features](#overview--key-features)
- [System Architecture](#system-architecture)
- [UML Class Diagram](#uml-class-diagram)
- [Entity-Relationship (ER) Diagram](#entity-relationship-er-diagram)
- [Sequence & Workflow Diagrams](#sequence--workflow-diagrams)
  - [Account Creation Workflow](#account-creation-workflow)
  - [Fund Transfer Workflow (Fail-Fast)](#fund-transfer-workflow-fail-fast)
- [Persistence & Storage Design](#persistence--storage-design)
  - [accounts.dat](#accountsdat)
  - [transactions.log](#transactionslog)
- [Security & Business Rules](#security--business-rules)
- [Compilation & Running](#compilation--running)
  - [Prerequisites](#prerequisites)
  - [Build Commands](#build-commands)
- [License](#license)
- [Author](#-author)

---

## Overview & Key Features

- **Account Management**: Open accounts with full customer names, 4–6 digit numeric PINs, and optional initial deposit amounts.
- **Fail-Fast Transaction Processing**: Authenticates accounts and verifies credentials immediately before requesting operational details (e.g., recipient ID or transfer amount), preventing unnecessary user input on invalid requests.
- **Financial Operations**:
  - **Cash Deposit**: Safely increments account balance and logs receipt.
  - **Cash Withdrawal**: Validates PIN and balance before debiting funds.
  - **Inter-Account Transfers**: Atomic balance deduction from sender and credit to receiver with dual transaction tracking.
  - **Balance Inquiry & Mini-Statement**: Displays real-time balances and full formatted chronological transaction histories.
  - **Account Closure**: Enforces zero-balance business rule (account must be emptied prior to deletion).
- **Persistent Storage**: Serializes accounts and complete transaction histories to disk (`accounts.dat`) so statements survive restarts.
- **Append-Only Audit Log**: Maintains an immutable text log (`transactions.log`) recording timestamps, account IDs, operation types, amounts, and final balances.
- **Cross-Platform Console UI**: Boxed ASCII menus and ANSI color schemes with automated detection of terminal interactivity (`isatty`) and Windows Virtual Terminal Processing support.

---

## System Architecture

The application adopts a clean, layered modular design separating Presentation, Domain Logic, and Storage Management within a unified, high-performance C++ implementation:

```mermaid
graph TD
    subgraph Presentation_Layer ["Presentation & UI Layer"]
        CLI["CLI Menu Loop & Action Handlers"]
        IO["Input/Output Helpers (readLine, readInt, readDouble)"]
        Term["Terminal Management (ANSI Colors, isatty detection)"]
    end

    subgraph Domain_Layer ["Core Domain & Business Logic"]
        Bank["Bank Controller / Service Engine"]
        Account["Account Entity"]
        Txn["Transaction Value Object"]
    end

    subgraph Persistence_Layer ["Data & Storage Layer"]
        AccStore[("accounts.dat<br/>(Master & History Storage)")]
        AuditLog[("transactions.log<br/>(Append-Only Audit Log)")]
    end

    CLI --> IO
    CLI --> Term
    CLI -->|Invokes Operations| Bank

    Bank -->|Aggregates & Manages| Account
    Account -->|Contains 1..*| Txn

    Bank -->|Loads / Writes| AccStore
    Bank -->|Appends Log Events| AuditLog
```

### Architectural Responsibilities

1. **Presentation Layer**: Handles user interaction, input parsing/type checking, formatted display tables, colored alerts, and terminal mode management.
2. **Domain Layer (`Bank`)**: Enforces transaction validity, PIN authentication, invariant checking (e.g., overdraft protection, account closure preconditions), and ID generation.
3. **Persistence Layer**: Implements custom pipe-delimited serialization and deserialization for account structures and append-only streaming for the compliance audit trail.

---

## UML Class Diagram

The following UML class diagram models the structural attributes, operations, access visibility, and relationships:

```mermaid
classDiagram
    direction TB

    class Transaction {
        +string timestamp
        +string type
        +double amount
        +double balanceAfter
        +string note
    }

    class Account {
        +int id
        +string name
        +string pin
        +double balance
        +vector~Transaction~ history
    }

    class Bank {
        -map~int, Account~ accounts
        -int nextId
        -string dataFile
        -string logFile
        -currentTimestamp() string$
        -appendLog(string line) void
        -recordTransaction(Account& acc, string type, double amount, string note) void
        -getAccountOrThrow(int id) Account&
        -authenticate(Account& acc, string pin) void
        +Bank()
        +~Bank()
        +loadAccounts() void
        +saveAccounts() void
        +isValidPin(string pin) bool$
        +isValidName(string name) bool$
        +verifyAccountAccess(int id, string pin) void
        +accountExists(int id) bool
        +createAccount(string name, string pin, double initialDeposit) int
        +deposit(int id, string pin, double amount) void
        +withdraw(int id, string pin, double amount) void
        +transfer(int fromId, string pin, int toId, double amount) void
        +checkBalance(int id, string pin) double
        +getStatement(int id, string pin) Account
        +closeAccount(int id, string pin) void
    }

    Bank "1" *-- "0..*" Account : manages
    Account "1" *-- "0..*" Transaction : records
```

---

## Entity-Relationship (ER) Diagram

```mermaid
erDiagram
    BANK ||--o{ ACCOUNT : "maintains"
    ACCOUNT ||--o{ TRANSACTION : "owns"

    ACCOUNT {
        int id PK "Unique identifier (starts at 1001)"
        string name "Customer full name (no pipe characters)"
        string pin "4 to 6 digit numeric security PIN"
        double balance "Current settled balance"
    }

    TRANSACTION {
        string timestamp "Event execution timestamp (YYYY-MM-DD HH:MM:SS)"
        string type "OPEN | OPEN_DEPOSIT | DEPOSIT | WITHDRAW | TRANSFER_IN | TRANSFER_OUT"
        double amount "Transaction value"
        double balanceAfter "Account balance post-transaction"
        string note "Descriptive contextual note or counterparty ID"
    }
```

---

## Sequence & Workflow Diagrams

### Account Creation Workflow

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant UI as Menu Handler
    participant Bank as Bank Engine
    participant Acc as Account Entity
    participant Disk as accounts.dat
    participant Log as transactions.log

    User->>UI: Select "1 (Create Account)"
    UI->>User: Prompt Name, PIN, Initial Deposit
    User-->>UI: Enter details
    UI->>Bank: createAccount(name, pin, initialDeposit)
    Bank->>Bank: Validate Name & PIN
    Bank->>Acc: Instantiate Account(nextId++, name, pin)
    alt initialDeposit > 0
        Bank->>Acc: balance += initialDeposit
        Bank->>Acc: recordTransaction("OPEN_DEPOSIT", amount)
    else initialDeposit == 0
        Bank->>Acc: recordTransaction("OPEN", 0.0)
    end
    Bank->>Log: Append to transactions.log
    Bank->>Disk: saveAccounts()
    Bank-->>UI: Return new Account ID
    UI-->>User: Display Success with Account ID
```

### Fund Transfer Workflow (Fail-Fast)

Demonstrating the fail-fast security design where the origin account and PIN are authenticated prior to prompting for counterparty account or transfer value:

```mermaid
sequenceDiagram
    autonumber
    actor User
    participant UI as Menu Handler
    participant Bank as Bank Engine
    participant Disk as accounts.dat
    participant Log as transactions.log

    User->>UI: Select "4 (Transfer Funds)"
    UI->>User: Prompt Origin Account ID & PIN
    User-->>UI: Input fromId, pin
    UI->>Bank: verifyAccountAccess(fromId, pin)

    alt Invalid ID or PIN
        Bank-->>UI: Throw Exception
        UI-->>User: Display Error and Return to Menu
    else Valid Credentials
        Bank-->>UI: Access Granted
        UI->>User: Prompt Destination Account ID
        User-->>UI: Input toId
        UI->>Bank: accountExists(toId)
        alt Destination Does Not Exist
            Bank-->>UI: false
            UI-->>User: Display "Destination account does not exist"
        else Destination Exists
            UI->>User: Prompt Transfer Amount
            User-->>UI: Input amount
            UI->>Bank: transfer(fromId, pin, toId, amount)
            Bank->>Bank: Verify from.balance >= amount
            Bank->>Bank: from.balance -= amount
            Bank->>Bank: to.balance += amount
            Bank->>Log: Append TRANSFER_OUT and TRANSFER_IN entries
            Bank->>Disk: saveAccounts()
            Bank-->>UI: Success
            UI-->>User: Show Confirmation & Updated Balance
        end
    end
```

---

## Persistence & Storage Design

### `accounts.dat`
Stores full state including account definitions and historical ledger records. Data is stored in a line-based format where transaction records follow their respective account parent:

```text
ACCOUNT|<id>|<name>|<pin>|<balance>
TXN|<timestamp>|<type>|<amount>|<balanceAfter>|<note>
TXN|<timestamp>|<type>|<amount>|<balanceAfter>|<note>
```

#### Example:
```text
ACCOUNT|1001|Alice Smith|1234|450.00
TXN|2026-09-10 14:02:11|OPEN_DEPOSIT|500.00|500.00|Account opening deposit
TXN|2026-09-10 14:15:30|WITHDRAW|50.00|450.00|Cash withdrawal
ACCOUNT|1002|Bob Jones|5678|1050.00
TXN|2026-09-10 14:05:00|OPEN_DEPOSIT|1000.00|1000.00|Account opening deposit
TXN|2026-09-10 14:20:00|DEPOSIT|50.00|1050.00|Cash deposit
```

### `transactions.log`
An immutable, append-only log created for administrative auditing, reconciliation, and compliance:

```text
YYYY-MM-DD HH:MM:SS | Acc#<id> | <TYPE> | Amount: <amount> | Balance: <balance> | <note>
```

#### Example:
```text
2026-09-10 14:02:11 | Acc#1001 | OPEN_DEPOSIT | Amount: 500.00 | Balance: 500.00 | Account opening deposit
2026-09-10 14:15:30 | Acc#1001 | WITHDRAW | Amount: 50.00 | Balance: 450.00 | Cash withdrawal
2026-09-10 14:25:12 | Acc#1001 | TRANSFER_OUT | Amount: 100.00 | Balance: 350.00 | To Acc#1002
2026-09-10 14:25:12 | Acc#1002 | TRANSFER_IN | Amount: 100.00 | Balance: 1150.00 | From Acc#1001
```

---

## Security & Business Rules

| Rule | Description |
| :--- | :--- |
| **PIN Constraints** | Must consist strictly of 4 to 6 numeric digits (`0-9`). |
| **Name Validation** | Must be non-empty and cannot contain the delimiter character (`\|`) to prevent serialization injection. |
| **Fail-Fast Authentication** | Operations requiring credentials check the account ID and PIN before taking supplementary inputs. |
| **Overdraft Protection** | Withdrawals and outgoing transfers check that `amount <= balance`; negative balances are forbidden. |
| **Self-Transfer Prevention** | Transfers where `fromId == toId` are rejected. |
| **Account Closure Condition** | An account can only be deleted/closed if its settled balance is exactly `0.00`. |

---

## Compilation & Running

### Prerequisites
- A C++17 compatible compiler (e.g., `g++` 8.0+, `clang++` 7.0+, or MSVC 2019+).

### Build Commands

#### Linux / macOS
```bash
# Compile
g++ -std=c++17 -O2 -Wall -Wextra -o bankingSystem bankingSystem.cpp

# Run
./bankingSystem
```

#### Windows (MinGW-w64 / GCC)
```powershell
# Compile
g++ -std=c++17 -O2 -Wall -Wextra -o bankingSystem.exe bankingSystem.cpp

# Run
.\bankingSystem.exe
```

#### Windows (MSVC Developer Command Prompt)
```cmd
# Compile
cl /std:c++17 /O2 /W4 /EHsc bankingSystem.cpp /Fe:bankingSystem.exe

# Run
bankingSystem.exe
```

---

## License

This project is licensed under the [MIT License](LICENSE).

---

## 👨‍💻 Author

Developed and maintained by **[Tanish](https://github.com/tanishpal23)**.

- **GitHub Profile**: [Tanish](https://github.com/tanishpal23)
- **Project Repository**: [Banking System](https://github.com/Tanishpal23/Banking-System)

---

<p align="center">
  Made with ❤️ by <a href="https://github.com/tanishpal23"><strong>Tanish</strong></a>
</p>

