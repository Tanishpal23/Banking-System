# Core Banking System

A simple, self-contained core banking system with a colorized console UI, written in C++17.

## Features

- Create accounts (name + PIN + optional initial deposit)
- Deposit / Withdraw / Transfer funds (PIN-protected, fails fast on a bad account or PIN before asking for further input)
- Balance inquiry and full mini-statement (transaction history is persisted to disk, so it survives a restart)
- Close account (only when balance is zero)
- Persistent storage to disk (`accounts.dat`) so data survives restarts
- Append-only audit log (`transactions.log`)
- Boxed, colorized, screen-based menu UI (falls back to plain text automatically when output isn't an interactive terminal)

## Quickstart (after cloning from GitHub)

**macOS / Linux:**
```bash
git clone https://github.com/<your-username>/<repo-name>.git
cd <repo-name>
./run.sh
```

**Windows:**
```bat
git clone https://github.com/<your-username>/<repo-name>.git
cd <repo-name>
run.bat
```

`run.sh` / `run.bat` compile the program automatically on first run (or whenever the source changes) and then launch it — one command, no separate build step needed. Requires a C++17 compiler (`g++`) on your PATH; on Windows this typically means installing [MinGW-w64](https://www.mingw-w64.org/) or using WSL.

## Manual build/run

If you'd rather build it yourself instead of using the scripts:

```bash
g++ -std=c++17 -O2 -Wall -Wextra -o core_banking_system core_banking_system.cpp
./core_banking_system
```

On first run it will create `accounts.dat` (account data) and `transactions.log` (audit log) in the working directory.

## License

MIT
# Banking-System
