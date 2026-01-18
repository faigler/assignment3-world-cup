#!/usr/bin/env python3
"""
Basic Python Server for STOMP Assignment – Stage 3.3

IMPORTANT:
DO NOT CHANGE the server name or the basic protocol.
Students should EXTEND this server by implementing
the methods below.
"""

import socket
import sqlite3
import sys
import threading


SERVER_NAME = "STOMP_PYTHON_SQL_SERVER"  # DO NOT CHANGE!
DB_FILE = "stomp_server.db"              # DO NOT CHANGE!



def recv_null_terminated(sock: socket.socket) -> str:
    data = b""
    while True:
        chunk = sock.recv(1024)
        if not chunk:
            return ""
        data += chunk
        if b"\0" in data:
            msg, _ = data.split(b"\0", 1)
            return msg.decode("utf-8", errors="replace")



# Initializes the SQLite database and required tables
def init_database():
    print(f"[{SERVER_NAME}] Initializing database...")
    
    # Open a database connection and automatically close it
    # when leaving the block (even if an error occurs)
    with sqlite3.connect(DB_FILE) as conn:
        # Execute multiple SQL commands at once
        conn.executescript("""
            CREATE TABLE IF NOT EXISTS Users (
                username TEXT PRIMARY KEY,
                password TEXT NOT NULL
            );

            CREATE TABLE IF NOT EXISTS Logins (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                login_time TEXT NOT NULL,
                logout_time TEXT
            );

            CREATE TABLE IF NOT EXISTS Files (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                username TEXT NOT NULL,
                filename TEXT NOT NULL,
                upload_time TEXT
            );
        """)

    print(f"[{SERVER_NAME}] Database initialized successfully")




# Executes SQL commands that MODIFY the database (INSERT / UPDATE / DELETE)
def execute_sql_command(sql_command: str) -> str:
    try:
        with sqlite3.connect(DB_FILE) as conn:
            conn.execute(sql_command)  # execute SQL command
            conn.commit()              # save changes
        return "done"
    except sqlite3.Error as e:
        return f"SQL ERROR: {e}"
    



# Executes SQL SELECT queries and returns results as a string
def execute_sql_query(sql_query: str) -> str:
    try:
        with sqlite3.connect(DB_FILE) as conn:
            cursor = conn.cursor()
            cursor.execute(sql_query) # run SELECT query
            rows = cursor.fetchall() # get all results

        if not rows:
            return "" # no results

        # Convert rows to string format
        output_lines = []
        for row in rows:
            output_lines.append(" ".join(str(item) for item in row))

        # Separate rows with '|'
        return "|".join(output_lines)

    except sqlite3.Error as e:
        return f"SQL ERROR: {e}"
    


# Prints a server-side report (for debugging)
def print_server_report():
    print("\n--- SERVER REPORT ---")

    try:
        with sqlite3.connect(DB_FILE) as conn:
            cursor = conn.cursor()

            # Print registered users
            print("1. Registered Users:")
            cursor.execute("SELECT username FROM Users")
            users = cursor.fetchall()
            if not users:
                print("   (None)")
            for u in users:
                print(f"   - {u[0]}")

            # Print login history
            print("\n2. Login History:")
            cursor.execute("SELECT username, login_time, logout_time FROM Logins")
            logins = cursor.fetchall()
            if not logins:
                print("   (None)")
            for row in logins:
                logout = row[2] if row[2] else "Active"
                print(f"   - User: {row[0]}, Login: {row[1]}, Logout: {logout}")

            # Print uploaded files
            print("\n3. Uploaded Files:")
            cursor.execute("SELECT username, filename FROM Files")
            files = cursor.fetchall()
            if not files:
                print("   (None)")
            for row in files:
                print(f"   - User: {row[0]}, File: {row[1]}")

    except sqlite3.Error as e:
        print(f"Error printing report: {e}")

    print("---------------------\n")



# Handles a single client connection
def handle_client(client_socket: socket.socket, addr):
    print(f"[{SERVER_NAME}] Client connected from {addr}")

    try:
        while True:
            # Receive one null-terminated message
            message = recv_null_terminated(client_socket)
            if message == "":
                break  # client disconnected

            print(f"[{SERVER_NAME}] Received:")
            print(message)

            clean_msg = message.strip().upper()

            # Special server command
            if clean_msg == "REPORT":
                print_server_report()
                response = "Report printed on server console"

            # SQL SELECT query
            elif clean_msg.startswith("SELECT"):
                response = execute_sql_query(message)

            # Any other SQL command
            else:
                response = execute_sql_command(message)

            # Send response back to client
            client_socket.sendall(response.encode("utf-8") + b"\0")

    except Exception as e:
        print(f"[{SERVER_NAME}] Error handling client {addr}: {e}")

    finally:
        client_socket.close()
        print(f"[{SERVER_NAME}] Client {addr} disconnected")





# Starts the server and accepts incoming connections
def start_server(host="127.0.0.1", port=7778):
    init_database() # opens the database and creates tables
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server_socket.bind((host, port))
        server_socket.listen(5)
        print(f"[{SERVER_NAME}] Server started on {host}:{port}")
        print(f"[{SERVER_NAME}] Waiting for connections...")

        while True:
            client_socket, addr = server_socket.accept()
            t = threading.Thread(
                target=handle_client,
                args=(client_socket, addr),
                daemon=True
            )
            t.start()

    except KeyboardInterrupt:
        print(f"\n[{SERVER_NAME}] Shutting down server...")
    finally:
        try:
            server_socket.close()
        except Exception:
            pass


if __name__ == "__main__":
    port = 7778
    if len(sys.argv) > 1:
        raw_port = sys.argv[1].strip()
        try:
            port = int(raw_port)
        except ValueError:
            print(f"Invalid port '{raw_port}', falling back to default {port}")

    start_server(port=port)
