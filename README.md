# Gr6-ProgramimiMeSockets-TCP

# Server–Client File Management System  
Projekt për komunikim TCP mes Serverit dhe Klientëve me privilegje të ndryshme (Admin & Read-Only).

## Përshkrimi  
Ky projekt implementon një aplikacion **Server–Client në C++** duke përdorur **TCP Sockets (Winsock2)**.  
Klientët lidhen me serverin dhe ekzekutojnë komanda për menaxhimin e file-ve.

Ekzistojnë dy role:
- **Admin Client** – qasje e plotë (read/write/execute)
- **Regular Client** – read-only permission

Serveri menaxhon shumë lidhje në të njëjtën kohë duke përdorur **multithreading**.

---

## Funksionalitetet – Serveri

### Komandat e mbështetura:
| Komanda | Përshkrimi |
|--------|------------|
| `/list` | Liston të gjithë file-at në directory të serverit |
| `/read <filename>` | Lexon përmbajtjen e një file-i |
| `/upload <filename>` | Pranon një file nga klienti (admin only) |
| `/download <filename>` | Kthen file-in te klienti për shkarkim |
| `/delete <filename>` | Fshin një file nga serveri (admin only) |
| `/search <keyword>` | Kërkon file sipas fjalës kyçe |
| `/info <filename>` | Shfaq madhësinë dhe datat e krijimit/modifikimit |

Serveri:
- përdor një IP dhe port të caktuar,
- pranon më së shumti 4 klientë,
- kontrollon privilegjet e secilit klient,
- ruan file-t në një directory të dedikuar.

---

##  Funksionalitetet – Klienti

### 1. Lidhja
- Krijon socket
- Lidhet me IP dhe portin e serverit
- Dërgon komanda me tekst
- Lexon përgjigjet nga serveri

### 2. Privilegjet

####  Admin Client
Ka qasje në të gjitha komandat
####  Regular Client (Read-Only)
Lejohen vetëm: `/list`, `/read <filename>`, `/download <filename>`, `/search <keyword>`, `/info <filename>`.
Nuk ka qasje në: `/upload`, `/delete`, `/execute`.

---

## Teknologjitë e përdorura
- C++
- Windows Winsock2
- TCP Socket Programming
- Multithreading
- File I/O në server

---

## Struktura e projektit

- /project
- /server
- server.cpp
- /files (directory i file-ve të serverit)
- /client
- client.cpp
- README.md

---

##  Si të ekzekutohet?

### 1. Start serverin:
- ./server.exe

### 2. Start klientin:
- ./client.exe

- Pastaj jep një rol:
    - 1 = Admin
    - 0 = Regular client

- Dhe shkruan komandat p.sh.: `/list`, `/read test.txt`, `/download data.bin`.

---

## Permissions & Performance
- Admini → qasje e plotë dhe kohë më të shpejtë përgjigjeje  
- Regular client → read-only  
- Serveri menaxhon çdo klient në thread të veçantë

---

## Statusi i projektit
✔ Server funksional  
✔ Klient me dy role (admin & read-only)  
✔ Komandat kryesore të implementuara  
✔ Transferim file-sh (download/upload) funksional  

---


