#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

// 32-Byte Telemetry Data Layout packed strictly at byte-boundaries
struct __attribute__((packed)) TelemetryPacket {
    double latitude;
    double longitude;
    double altitude;
    uint64_t sequence_id;
};

extern "C" {
    // Exported symbol for direct foreign-function interface orchestration
    int run_ingress_listener(int port) {
        int server_fd = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        bind(server_fd, (struct sockaddr*)&address, sizeof(address));
        listen(server_fd, 128);

        while (true) {
            int client_socket = accept(server_fd, nullptr, nullptr);
            TelemetryPacket packet{};
            
            // Read direct memory layout directly off the wire with zero string-parsing overhead
            ssize_t bytes_read = recv(client_socket, &packet, sizeof(TelemetryPacket), MSG_WAITALL);
            if (bytes_read == sizeof(TelemetryPacket)) {
                // Pass directly to internal execution pipelines
                std::cout << "[C++ Core] Ingested Sequence: " << packet.sequence_id << "\n";
            }
            close(client_socket);
        }
        close(server_fd);
        return 0;
    }
}
