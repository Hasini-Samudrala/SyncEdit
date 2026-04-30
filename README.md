# SyncEdit

## 📌 Overview

SyncEdit is a multi-user collaborative system that allows multiple clients to access and modify a shared file in a controlled and synchronized manner. The system ensures data consistency using locking, supports real-time updates, maintains version history, and includes role-based access with admin override capabilities.

---

## 🚀 Features

* **Concurrent Multi-Client Support**
  Multiple users can connect simultaneously using socket-based communication.

* **Synchronized Editing (Locking Mechanism)**
  Only one user can edit the file at a time using a controlled access system.

* **Request Queue (FIFO Scheduling)**
  Users request edit access and are placed in a queue to ensure fairness.

* **Admin Override (Priority Control)**
  Admin can preempt the current editor using `force_edit`. Interrupted users resume editing before others.

* **Live View Mode**
  Users can subscribe to real-time updates of the shared file.

* **Version History**
  Every edit creates a snapshot stored in the `versions/` directory.

* **Version Retrieval & Diff**
  Users can view previous versions and compare differences.

* **Chat System**
  Clients can communicate with each other via broadcast messaging.

---

## 🧩 System Design

* **Server**

  * Handles multiple clients using threads (`pthread`)
  * Maintains shared state:

    * current editor
    * request queue
    * live subscribers
  * Ensures synchronization using mutex locks

* **Client**

  * Uses a separate thread to receive messages
  * Supports interactive command-based input

---

## ⚙️ Technologies Used

* **Language:** C
* **Concepts:**

  * Socket Programming
  * Multithreading
  * Synchronization (Mutex Locks)
  * File Handling
  * Queue Scheduling

---

## 📂 Project Structure

```text
project/
│── server_with_roles_final.c
│── client_f.c
│── shared.txt
│── versions/
│    ├── v1.txt
│    ├── v2.txt
│    └── ...
```

---

## ▶️ How to Run

### 1. Compile

```bash
gcc server.c -o server -lpthread
gcc client.c -o client -lpthread
```

### 2. Run Server

```bash
./server
```

### 3. Run Clients (multiple terminals)

```bash
./client
```

---

## 🧾 Commands

| Command      | Description                   |
| ------------ | ----------------------------- |
| `view`       | View current file             |
| `live`       | Enable live updates           |
| `exit_live`  | Disable live mode             |
| `request`    | Request edit access           |
| `edit`       | Edit file (only when allowed) |
| `force_edit` | Admin override                |
| `history`    | Show version list             |
| `version n`  | View specific version         |
| `diff a b`   | Compare versions              |
| `exit`       | Exit client                   |

---

## 🔐 Roles

* **Editor (default):**

  * Can request and edit files
  * Must follow queue system

* **Admin:**

  * Authenticated using password (`admin1234`)
  * Can override current editor using `force_edit`
  * Interrupted editors regain priority

---

## 🧠 Key Concepts Demonstrated

* Mutual Exclusion (Critical Section Handling)
* Process Scheduling (FIFO Queue)
* Inter-Process Communication (Sockets)
* Real-Time Data Synchronization
* Preemptive Resource Allocation (Admin Override)

---

## 🎯 Conclusion

This project simulates a real-world collaborative editing system with strong emphasis on operating system concepts such as synchronization, scheduling, and concurrency control. It demonstrates how multiple users can safely interact with shared resources in a controlled environment.

---

