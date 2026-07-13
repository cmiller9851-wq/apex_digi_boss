OPENQASM 3.0;
include "stdgates.inc";

// Allocate physical hardware execution register elements explicitly
qubit[3] q;
bit[3] classical_telemetry;

// Initialize hardware phase operations
h q[0];
cx q[0], q[1];
cx q[1], q[2];

// Trap real physical state parameters directly into memory register arrays
classical_telemetry[0] = measure q[0];
classical_telemetry[1] = measure q[1];
classical_telemetry[2] = measure q[2];
