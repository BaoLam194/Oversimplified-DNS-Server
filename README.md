# 🧩 DNS Server — Codecrafters.io Challenge

This project is my implementation of a DNS server built from scratch as part of the [**"Build Your Own DNS Server"** challenge on Codecrafters.io](https://app.codecrafters.io/courses/dns-server/overview).

I implemented DNS packet parsing, response construction, and server logic based on the initial scaffold provided in the challenge.

---

## 📣 Developer Notes

Although this is called a “DNS Server,” the real heavy lifting in resolution is delegated to an upstream resolver 😅.  
Because of that, I consider this more of an **oversimplified DNS server / DNS forwarder**.

My main tasks for this project were:

- Parsing raw binary DNS packets into my own internal data structures  
- Following the DNS specification defined in [**RFC 1035**](https://www.rfc-editor.org/rfc/rfc1035)  
- Forwarding queries to an upstream resolver  
- Receiving and relaying responses back to the client  

This challenge helped me understand low-level networking, binary packet encoding/decoding, and DNS internals.

## 🚀 How the Server Works

### 1. Receiving the Query

The server listens on a UDP socket. When a DNS query arrives, it:

- Parses the 12-byte DNS header
- Reads and decodes the question section
- Validates supported query types (e.g., `A` records)

### 2. Forwarding the Query

Because the server can not resolver locally, it acts as a **DNS forwarder**:

- Sends the raw query packet to an upstream resolver (e.g., `1.1.1.1:53`, which could be changed for flexibility)
- Waits for a response 
- Relays the upstream response back to the originating client

When forwarding, the server preserves:
- Transaction ID
- Flags
- Question section
  
This ensures RFC-compliant behavior and proper client-side validation. But as part of [Codecrafters.io](https://app.codecrafters.io/courses/dns-server/stages/gt1) Challenge, we will seperate the mutiple question query into mutiple single question query and send to the upstream resolver. Later, we will collect all the response from the upstream resolver and craft a mutiple question and answer response to the original client.

### 3. Constructing the Response

Whether using a local answer or the upstream resolver's result, the server assembles a valid DNS response:

- DNS Header
- Question section
- Answer section

All fields are encoded manually, byte-by-byte, following the **RFC 1035 wire format**.

The constructed packet is then returned to the client via UDP.
## ⚠️ Limitations

- **Single-threaded / blocking design:**  
  Currently, the server handles only one query at a time. A single client request will block the entire process, so concurrent DNS queries are not supported yet. Improving this with asynchronous I/O or multi-threading is planned for the future.

- **Minimal, learning-focused implementation:**  
  This project is intentionally simplified to focus on understanding DNS mechanics. Many real-world concerns (e.g., caching, rate limiting, security hardening, TCP fallback, full record type support) are not implemented.  
  As a result, the server may be vulnerable to certain attacks or malformed input in a production environment. So it may not be production-ready.
