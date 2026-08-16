// bankingSystem.cpp
// A simple, self-contained core banking system with a colorized console UI.
//
// Features:
//   - Create accounts (name + PIN + optional initial deposit)
//   - Deposit / Withdraw / Transfer funds (PIN-protected, fails fast on bad
//     account/PIN before asking for further input)
//   - Balance inquiry and full mini-statement (transaction history is
//     persisted to disk, so it survives a restart)
//   - Close account (only when balance is zero)
//   - Persistent storage to disk (accounts.dat) so data survives restarts
//   - Append-only audit log (transactions.log)
//   - Boxed, colorized, screen-based menu UI (falls back to plain text
//     automatically when output isn't an interactive terminal)
//
// Build:
//   g++ -std=c++17 -O2 -Wall -Wextra -o bankingSystem bankingSystem.cpp
// Run:
//   ./bankingSystem

#include<bits/stdc++.h>
using namespace std;

#ifdef _WIN32
    #include <io.h>
    #include <windows.h>
    #define ISATTY _isatty
    #define FILENO _fileno
#else
    #include <unistd.h>
    #define ISATTY isatty
    #define FILENO fileno
#endif


// ---------------------------------------------------------------------------
// Terminal capability detection + colour helpers
// ---------------------------------------------------------------------------

namespace Color {
    const string RESET   = "\033[0m";
    const string BOLD    = "\033[1m";
    const string DIM     = "\033[2m";
    const string RED     = "\033[31m";
    const string GREEN   = "\033[32m";
    const string YELLOW  = "\033[33m";
    const string BLUE    = "\033[34m";
    const string MAGENTA = "\033[35m";
    const string CYAN    = "\033[36m";
    const string WHITE   = "\033[37m";
}

bool gInteractive = true; // both stdin and stdout are real terminals

bool detectInteractive() {
    return ISATTY(FILENO(stdout)) && ISATTY(FILENO(stdin));
}

// Returns the colour code if running in an interactive terminal, otherwise
// returns an empty string so redirected/piped output stays clean.
string C(const string &code) {
    return gInteractive ? code : "";
}

void enableAnsiSupport() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (hOut != INVALID_HANDLE_VALUE && GetConsoleMode(hOut, &mode)) {
        SetConsoleMode(hOut, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

void clearScreen() {
    if (!gInteractive) return;
#ifdef _WIN32
    if (system("cls") != 0) { /* non-fatal: worst case, screen isn't cleared */ }
#else
    if (system("clear") != 0) { /* non-fatal: worst case, screen isn't cleared */ }
#endif
}

// Repeats a glyph n times, e.g. repeatGlyph("-", 40).
// Plain ASCII is used everywhere (instead of Unicode box-drawing characters)
// because many consoles -- Windows cmd.exe in particular -- don't default to
// a UTF-8 code page, which turns multi-byte box characters into garbled text.
string repeatGlyph(const string &glyph, int n) {
    string out;
    out.reserve(glyph.size() * static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) out += glyph;
    return out;
}

string formatMoney(double amount) {
    ostringstream oss;
    oss << fixed << setprecision(2) << amount;
    return oss.str();
}

// ---------------------------------------------------------------------------
// Data model
// ---------------------------------------------------------------------------

struct Transaction {
    string timestamp;
    string type;          // OPEN, OPEN_DEPOSIT, DEPOSIT, WITHDRAW, TRANSFER_IN, TRANSFER_OUT
    double amount;
    double balanceAfter;
    string note;
};

struct Account {
    int id = 0;
    string name;
    string pin;
    double balance = 0.0;
    vector<Transaction> history;
};

// ---------------------------------------------------------------------------
// Bank: owns all accounts, persistence, and business rules
// ---------------------------------------------------------------------------

class Bank {
private:
    map<int, Account> accounts;
    int nextId;
    const string dataFile = "accounts.dat";
    const string logFile  = "transactions.log";

    static string currentTimestamp() {
        time_t now = time(nullptr);
        char buf[32];
        strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
        return string(buf);
    }

    void appendLog(const string &line) {
        ofstream out(logFile, ios::app);
        if (out) out << line << "\n";
    }

    // Updates history + audit log. Caller MUST update acc.balance BEFORE
    // calling this, so that balanceAfter reflects the post-transaction state.
    void recordTransaction(Account &acc, const string &type, double amount, const string &note) {
        Transaction t{currentTimestamp(), type, amount, acc.balance, note};
        acc.history.push_back(t);

        ostringstream oss;
        oss << t.timestamp << " | Acc#" << acc.id << " | " << type
            << " | Amount: " << formatMoney(amount)
            << " | Balance: " << formatMoney(acc.balance)
            << " | " << note;
        appendLog(oss.str());
    }

    Account &getAccountOrThrow(int id) {
        auto it = accounts.find(id);
        if (it == accounts.end())
            throw invalid_argument("Account #" + to_string(id) + " does not exist.");
        return it->second;
    }

    void authenticate(Account &acc, const string &pin) {
        if (acc.pin != pin)
            throw invalid_argument("Incorrect PIN for account #" + to_string(acc.id) + ".");
    }

public:
    Bank() : nextId(1001) {
        loadAccounts();
    }

    ~Bank() {
        saveAccounts();
    }

    // -- Persistence ---------------------------------------------------
    // accounts.dat line formats:
    //   ACCOUNT|id|name|pin|balance
    //   TXN|timestamp|type|amount|balanceAfter|note      (belongs to the
    //                                                      preceding ACCOUNT)
    // Storing full history here (not just the audit log) is what lets the
    // mini-statement survive a program restart.

    void loadAccounts() {
        ifstream in(dataFile);
        if (!in) return;

        string line;
        int maxId = 1000;
        Account *current = nullptr;

        while (getline(in, line)) {
            if (line.empty()) continue;
            istringstream iss(line);
            string tag;
            getline(iss, tag, '|');

            if (tag == "ACCOUNT") {
                string field;
                Account acc;
                if (!getline(iss, field, '|')) continue;
                acc.id = stoi(field);
                if (!getline(iss, field, '|')) continue;
                acc.name = field;
                if (!getline(iss, field, '|')) continue;
                acc.pin = field;
                if (!getline(iss, field, '|')) continue;
                acc.balance = stod(field);

                accounts[acc.id] = acc;
                current = &accounts[acc.id];
                if (acc.id > maxId) maxId = acc.id;

            } else if (tag == "TXN" && current != nullptr) {
                string field;
                Transaction t;
                if (!getline(iss, field, '|')) continue;
                t.timestamp = field;
                if (!getline(iss, field, '|')) continue;
                t.type = field;
                if (!getline(iss, field, '|')) continue;
                t.amount = stod(field);
                if (!getline(iss, field, '|')) continue;
                t.balanceAfter = stod(field);
                getline(iss, field);
                t.note = field; // remainder of the line
                current->history.push_back(t);
            }
        }
        nextId = maxId + 1;
    }

    void saveAccounts() {
        ofstream out(dataFile, ios::trunc);
        for (auto &pr : accounts) {
            Account &a = pr.second;
            out << "ACCOUNT|" << a.id << "|" << a.name << "|" << a.pin << "|"
                << formatMoney(a.balance) << "\n";
            for (auto &t : a.history) {
                out << "TXN|" << t.timestamp << "|" << t.type << "|"
                    << formatMoney(t.amount) << "|" << formatMoney(t.balanceAfter) << "|"
                    << t.note << "\n";
            }
        }
    }

    // -- Validation helpers ---------------------------------------------

    static bool isValidPin(const string &pin) {
        if (pin.size() < 4 || pin.size() > 6) return false;
        for (char c : pin)
            if (!isdigit(static_cast<unsigned char>(c))) return false;
        return true;
    }

    static bool isValidName(const string &name) {
        if (name.empty() || name.find('|') != string::npos) return false;
        return true;
    }

    // -- Core operations --------------------------------------------------

    // Cheap, side-effect-free check used to fail fast (before collecting
    // amounts / destination accounts / etc.) if the account or PIN is bad.
    void verifyAccountAccess(int id, const string &pin) {
        Account &acc = getAccountOrThrow(id);
        authenticate(acc, pin);
    }

    bool accountExists(int id) const {
        return accounts.find(id) != accounts.end();
    }

    int createAccount(const string &name, const string &pin, double initialDeposit) {
        if (!isValidName(name))
            throw invalid_argument("Name must be non-empty and cannot contain '|'.");
        if (!isValidPin(pin))
            throw invalid_argument("PIN must be 4-6 digits.");
        if (initialDeposit < 0)
            throw invalid_argument("Initial deposit cannot be negative.");

        Account acc;
        acc.id = nextId++;
        acc.name = name;
        acc.pin = pin;
        acc.balance = 0.0;
        accounts[acc.id] = acc;

        Account &stored = accounts[acc.id];
        if (initialDeposit > 0.0) {
            stored.balance += initialDeposit;
            recordTransaction(stored, "OPEN_DEPOSIT", initialDeposit, "Account opening deposit");
        } else {
            recordTransaction(stored, "OPEN", 0.0, "Account opened");
        }

        saveAccounts();
        return stored.id;
    }

    void deposit(int id, const string &pin, double amount) {
        if (amount <= 0.0) throw invalid_argument("Deposit amount must be positive.");
        Account &acc = getAccountOrThrow(id);
        authenticate(acc, pin);

        acc.balance += amount;
        recordTransaction(acc, "DEPOSIT", amount, "Cash deposit");
        saveAccounts();
    }

    void withdraw(int id, const string &pin, double amount) {
        if (amount <= 0.0) throw invalid_argument("Withdrawal amount must be positive.");
        Account &acc = getAccountOrThrow(id);
        authenticate(acc, pin);
        if (amount > acc.balance) throw invalid_argument("Insufficient funds.");

        acc.balance -= amount;
        recordTransaction(acc, "WITHDRAW", amount, "Cash withdrawal");
        saveAccounts();
    }

    void transfer(int fromId, const string &pin, int toId, double amount) {
        if (amount <= 0.0) throw invalid_argument("Transfer amount must be positive.");
        if (fromId == toId) throw invalid_argument("Cannot transfer to the same account.");

        Account &from = getAccountOrThrow(fromId);
        authenticate(from, pin);
        Account &to = getAccountOrThrow(toId);

        if (amount > from.balance) throw invalid_argument("Insufficient funds.");

        from.balance -= amount;
        recordTransaction(from, "TRANSFER_OUT", amount, "To Acc#" + to_string(toId));

        to.balance += amount;
        recordTransaction(to, "TRANSFER_IN", amount, "From Acc#" + to_string(fromId));

        saveAccounts();
    }

    double checkBalance(int id, const string &pin) {
        Account &acc = getAccountOrThrow(id);
        authenticate(acc, pin);
        return acc.balance;
    }

    const Account &getStatement(int id, const string &pin) {
        Account &acc = getAccountOrThrow(id);
        authenticate(acc, pin);
        return acc;
    }

    void closeAccount(int id, const string &pin) {
        Account &acc = getAccountOrThrow(id);
        authenticate(acc, pin);
        if (acc.balance != 0.0)
            throw invalid_argument("Withdraw the full balance before closing the account.");

        ostringstream oss;
        oss << currentTimestamp() << " | Acc#" << id << " | CLOSE | Account closed";
        appendLog(oss.str());

        accounts.erase(id);
        saveAccounts();
    }
};

// ---------------------------------------------------------------------------
// Console I/O helpers
// ---------------------------------------------------------------------------

string readLine(const string &prompt) {
    cout << C(Color::CYAN) << prompt << C(Color::RESET);
    string line;
    getline(cin, line);
    return line;
}

int readInt(const string &prompt) {
    while (true) {
        cout << C(Color::CYAN) << prompt << C(Color::RESET);
        string line;
        getline(cin, line);
        try {
            size_t pos;
            int value = stoi(line, &pos);
            if (pos == line.size()) return value;
        } catch (...) {
            // fall through to error message
        }
        cout << C(Color::RED) << "  Invalid number, please try again." << C(Color::RESET) << "\n";
    }
}

double readDouble(const string &prompt) {
    while (true) {
        cout << C(Color::CYAN) << prompt << C(Color::RESET);
        string line;
        getline(cin, line);
        try {
            size_t pos;
            double value = stod(line, &pos);
            if (pos == line.size()) return value;
        } catch (...) {
            // fall through to error message
        }
        cout << C(Color::RED) << "  Invalid amount, please try again." << C(Color::RESET) << "\n";
    }
}

void printSuccess(const string &msg) {
    cout << C(Color::GREEN) << C(Color::BOLD) << "  [OK] " << C(Color::RESET)
         << C(Color::GREEN) << msg << C(Color::RESET) << "\n";
}

void printError(const string &msg) {
    cout << C(Color::RED) << C(Color::BOLD) << "  [X] " << C(Color::RESET)
         << C(Color::RED) << msg << C(Color::RESET) << "\n";
}

void printSectionHeader(const string &title) {
    cout << C(Color::BOLD) << C(Color::MAGENTA) << "-- " << title << " "
         << repeatGlyph("-", 40 - (int)title.size()) << C(Color::RESET) << "\n";
}

// Centers ASCII text within `width` columns (pads with spaces on both sides).
string centerText(const string &text, int width) {
    int pad = width - static_cast<int>(text.size());
    if (pad < 0) pad = 0;
    int left = pad / 2;
    int right = pad - left;
    return string(left, ' ') + text + string(right, ' ');
}

void printBanner() {
    const int width = 46;
    const string h = repeatGlyph("-", width);
    cout << C(Color::CYAN) << C(Color::BOLD);
    cout << "+" << h << "+\n";
    cout << "|" << centerText("C O R E   B A N K I N G   S Y S T E M", width) << "|\n";
    cout << "+" << h << "+\n";
    cout << C(Color::RESET);
}

void printMenu() {
    cout << C(Color::YELLOW) << C(Color::BOLD) << "\n  MAIN MENU\n" << C(Color::RESET);
    cout << C(Color::WHITE);
    cout << "   1  Create account\n"
         << "   2  Deposit funds\n"
         << "   3  Withdraw funds\n"
         << "   4  Transfer funds\n"
         << "   5  Check balance\n"
         << "   6  Mini statement\n"
         << "   7  Close account\n"
         << "   8  Exit\n";
    cout << C(Color::RESET);
    cout << C(Color::CYAN) << "\n  > Choose an option: " << C(Color::RESET);
}

void printStatement(const Account &acc) {
    cout << "\n";
    printSectionHeader("Statement: Acc#" + to_string(acc.id) + " (" + acc.name + ")");

    if (acc.history.empty()) {
        cout << "  No transactions yet.\n";
    } else {
        cout << C(Color::WHITE) << C(Color::BOLD)
             << "  " << left << setw(20) << "Date/Time"
             << setw(15) << "Type"
             << right << setw(10) << "Amount"
             << setw(12) << "Balance" << "   Note"
             << C(Color::RESET) << "\n";
        cout << "  " << repeatGlyph("-", 74) << "\n";

        for (const auto &t : acc.history) {
            bool isDebit = (t.type == "WITHDRAW" || t.type == "TRANSFER_OUT");
            string color = isDebit ? Color::RED : Color::GREEN;
            cout << C(color)
                 << "  " << left << setw(20) << t.timestamp
                 << setw(15) << t.type
                 << right << setw(10) << formatMoney(t.amount)
                 << setw(12) << formatMoney(t.balanceAfter)
                 << "   " << t.note
                 << C(Color::RESET) << "\n";
        }
    }
    cout << C(Color::BOLD) << "\n  Current balance: " << C(Color::RESET)
         << C(Color::GREEN) << C(Color::BOLD) << formatMoney(acc.balance)
         << C(Color::RESET) << "\n";
}

void pauseForUser() {
    if (!gInteractive) return;
    cout << C(Color::DIM) << "\n  Press Enter to continue..." << C(Color::RESET);
    string dummy;
    getline(cin, dummy);
}

// ---------------------------------------------------------------------------
// Menu action handlers
// Each one fails fast: if the account/PIN check fails, it returns
// immediately instead of asking for an amount / destination account first.
// ---------------------------------------------------------------------------

void handleCreateAccount(Bank &bank) {
    printSectionHeader("Create Account");
    string name = readLine("  Full name: ");
    string pin  = readLine("  Set a 4-6 digit PIN: ");
    double initial = readDouble("  Initial deposit (0 for none): ");
    try {
        int id = bank.createAccount(name, pin, initial);
        printSuccess("Account created. Account number: " + to_string(id));
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

void handleDeposit(Bank &bank) {
    printSectionHeader("Deposit Funds");
    int id = readInt("  Account number: ");
    string pin = readLine("  PIN: ");

    try {
        bank.verifyAccountAccess(id, pin); // early exit before asking for amount
    } catch (const exception &ex) {
        printError(ex.what());
        return;
    }

    double amt = readDouble("  Amount to deposit: ");
    try {
        bank.deposit(id, pin, amt);
        printSuccess("Deposit successful. New balance: " + formatMoney(bank.checkBalance(id, pin)));
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

void handleWithdraw(Bank &bank) {
    printSectionHeader("Withdraw Funds");
    int id = readInt("  Account number: ");
    string pin = readLine("  PIN: ");

    try {
        bank.verifyAccountAccess(id, pin); // early exit before asking for amount
    } catch (const exception &ex) {
        printError(ex.what());
        return;
    }

    double amt = readDouble("  Amount to withdraw: ");
    try {
        bank.withdraw(id, pin, amt);
        printSuccess("Withdrawal successful. New balance: " + formatMoney(bank.checkBalance(id, pin)));
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

void handleTransfer(Bank &bank) {
    printSectionHeader("Transfer Funds");
    int fromId = readInt("  From account number: ");
    string pin = readLine("  PIN: ");

    try {
        bank.verifyAccountAccess(fromId, pin); // early exit before asking for more
    } catch (const exception &ex) {
        printError(ex.what());
        return;
    }

    int toId = readInt("  To account number: ");
    if (!bank.accountExists(toId)) { // early exit before asking for amount
        printError("Destination account #" + to_string(toId) + " does not exist.");
        return;
    }

    double amt = readDouble("  Amount to transfer: ");
    try {
        bank.transfer(fromId, pin, toId, amt);
        printSuccess("Transfer successful. New balance: " + formatMoney(bank.checkBalance(fromId, pin)));
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

void handleCheckBalance(Bank &bank) {
    printSectionHeader("Check Balance");
    int id = readInt("  Account number: ");
    string pin = readLine("  PIN: ");
    try {
        double bal = bank.checkBalance(id, pin);
        cout << C(Color::GREEN) << C(Color::BOLD) << "  Balance: " << formatMoney(bal)
             << C(Color::RESET) << "\n";
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

void handleStatement(Bank &bank) {
    printSectionHeader("Mini Statement");
    int id = readInt("  Account number: ");
    string pin = readLine("  PIN: ");
    try {
        const Account &acc = bank.getStatement(id, pin);
        printStatement(acc);
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

void handleCloseAccount(Bank &bank) {
    printSectionHeader("Close Account");
    int id = readInt("  Account number: ");
    string pin = readLine("  PIN: ");
    try {
        bank.closeAccount(id, pin);
        printSuccess("Account closed successfully.");
    } catch (const exception &ex) {
        printError(ex.what());
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    enableAnsiSupport();
    gInteractive = detectInteractive();

    Bank bank;
    bool running = true;

    while (running) {
        clearScreen();
        printBanner();
        printMenu();

        string choiceLine;
        getline(cin, choiceLine);
        int choice = -1;
        try {
            size_t pos;
            choice = stoi(choiceLine, &pos);
            if (pos != choiceLine.size()) choice = -1;
        } catch (...) {
            choice = -1;
        }

        clearScreen();
        printBanner();

        switch (choice) {
            case 1: handleCreateAccount(bank); break;
            case 2: handleDeposit(bank); break;
            case 3: handleWithdraw(bank); break;
            case 4: handleTransfer(bank); break;
            case 5: handleCheckBalance(bank); break;
            case 6: handleStatement(bank); break;
            case 7: handleCloseAccount(bank); break;
            case 8:
                running = false;
                cout << C(Color::YELLOW) << "\n  Saving and exiting. Goodbye!\n" << C(Color::RESET);
                break;
            default:
                printError("Invalid option, please choose 1-8.");
        }

        if (running) pauseForUser();
    }

    return 0;
}