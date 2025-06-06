# 📸 ImgFS – A Simplified Haystack-Inspired Image Storage System

This project implements a simplified version of Facebook's Haystack photo storage system. It is designed to efficiently manage large volumes of images within a single custom-format file called `imgfs`, supporting metadata indexing, multi-resolution image storage, and deduplication. It addresses the metadata overhead issues of traditional file systems by storing images as "needles" within large, append-only "Haystack store files," significantly reducing filesystem metadata operations.

## 🚀 Project Overview

The goal was to build a CLI-based image storage utility that allows storing, listing, creating, and managing images in a compact and scalable format. The system mimics key concepts from Facebook’s Haystack architecture, focusing on performance, metadata minimization, and efficient I/O.

## ✅ Key Features Implemented

-   **Optimized Image Storage**: Stores images as "needles" within large, append-only "Haystack store files." This approach dramatically reduces the overhead associated with filesystem metadata operations, which is common in traditional file systems when managing billions of small files.
-   **Rapid Image Retrieval**: Achieves high-performance reads through a lean in-memory index. This allows for direct access to image data with a single I/O operation, bypassing typical filesystem traversals.
-   **Comprehensive Image Management**: Supports core functionalities including efficient writes, soft deletions, and compaction processes to reclaim unused space.
-   **Multi-Resolution Image Handling**: Automatically generates and manages different resolutions of images (original, small, and thumbnail). This ensures images are served optimally for various client needs.
-   **Content-Based Deduplication**: Employs SHA-256 hashing to identify and prevent the storage of identical image data, saving significant storage space.
-   **HTTP Server with RESTful API**: Provides a web interface to manage image operations, including listing, reading, deleting, and inserting images, accessible via standard HTTP requests.
-   **Concurrent Request Handling**: The HTTP server is multi-threaded, allowing it to process multiple client requests simultaneously, enhancing responsiveness and throughput.

## 🔧 Techniques 
-   **Custom Filesystem Implementation**: ImgFS implements a simplified file system abstraction over a standard POSIX filesystem. This involves managing custom header and metadata structures within a single file, mimicking low-level storage management.
-   **Lazy Resizing**: Images are resized to  `small`  and  `thumbnail`  resolutions only when they are first requested in those formats. This "lazy" approach saves computational resources and storage until a specific resolution is needed.
-   **Memory-Mapped File I/O**: Efficient data access is achieved by potentially using memory-mapped files for parts of the  `imgFS`  file, allowing direct memory access to file contents without traditional read/write calls.
-   **SHA-256 Hashing for Deduplication**: The use of SHA-256 hashes for content addressing is a robust method for detecting duplicate images, ensuring storage efficiency and data integrity.
-   **Simple HTTP Protocol Parsing**: The server includes a custom, simplified HTTP protocol parser (`http_prot.c`) tailored to the specific needs of the ImgFS API, demonstrating basic network protocol implementation.

## 💾 Libraries  

-   **libvips**: A high-performance image processing library used for image resizing and format conversion. It offers fast, low-memory operations suitable for large-scale image manipulation.
-   **json-c**: A lightweight JSON library for C that facilitates the parsing and generation of JSON data. This is used for the web API's  `list`  command, providing a structured output consumable by web clients.
-   **POSIX Threads (pthreads)**: The server leverages  `pthreads`  for multi-threading, enabling concurrent handling of client requests. This demonstrates fundamental concurrent programming patterns in C.
-   **Standard Unix Sockets (`sys/socket.h`)**: The network communication layer is built directly on Unix sockets, providing a low-level understanding and control over TCP/IP communication. This bypasses higher-level networking frameworks.

## 🛠 Example Usage

```bash
curl -i http://localhost:8000/imgfs                 # Should fail
curl -i http://localhost:8000/imgfs/read            # Should succeed
curl -i http://localhost:8000/imgfs/insert          # Should fail
curl -X POST -i http://localhost:8000/imgfs/insert  # Should succeed
```

