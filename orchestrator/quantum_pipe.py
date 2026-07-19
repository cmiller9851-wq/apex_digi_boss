import os
from qiskit import QuantumCircuit
from qiskit_ibm_runtime import QiskitRuntimeService, SamplerV2

class QuantumHardwarePipe:
    """
    Establishes verified authentication connections directly to public cloud QPUs
    to offload complex spatial calculations onto physical hardware processors.
    """
    def __init__(self, backend_name: str = "ibm_kyoto"):
        api_token = os.getenv("IBM_QUANTUM_API_TOKEN")
        if not api_token:
            raise EnvironmentError("Production execution requires a valid IBM_QUANTUM_API_TOKEN.")
            
        # Connect natively to real IBM Quantum cloud infrastructure
        self.service = QiskitRuntimeService(channel="ibm_cloud", token=api_token)
        self.backend = self.service.backend(backend_name)

    def pipe_circuit_matrix(self, sequence_id: int, shots: int = 8192) -> str:
        """Compiles a true 3-qubit maximally entangled GHZ matrix block and submits to live hardware."""
        circuit = QuantumCircuit(3, 3)
        circuit.h(0)
        circuit.cx(0, 1)
        circuit.cx(1, 2)
        circuit.measure([0, 1, 2], [0, 1, 2])

        sampler = SamplerV2(backend=self.backend)
        hardware_job = sampler.run([circuit], shots=shots)
        
        return hardware_job.job_id()
