import matplotlib.pyplot as plt
import os
import numpy as np

MPI_E = [1.0, 0.770, 0.295]

OMP_ATOMIC_E = [1.0, 0.660, 0.454]

OMP_PRIVATE_E = [1.0, 0.549, 0.256]

CUDA_RUNS = [0.328609, 0.304685, 0.270889]
CUDA_MEDIAN = np.median(CUDA_RUNS)
print(f"CUDA Median: {CUDA_MEDIAN}")

P = [1, 2, 4]

FIGURE_PATH = 'figures'

plt.plot(P, MPI_E, label='MPI_E', marker='o')
plt.plot(P, OMP_ATOMIC_E, label='OMP_ATOMIC_E', marker='o')
plt.plot(P, OMP_PRIVATE_E, label='OMP_PRIVATE_E', marker='o')
plt.title('Efficiency Comparison, r = 128, Stanford Bunny')
plt.xlabel('Number of Processes/Threads')
plt.ylabel('Efficiency')
plt.legend()
plt.grid()
plt.xticks(P)
plt.savefig(os.path.join(FIGURE_PATH, 'efficiency_comparison.png'))