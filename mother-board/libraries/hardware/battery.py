import numpy as np
import matplotlib.pyplot as plt

# Define the function
def f(v):
    return 100 / ((1 + (3.7 / v)**80)**0.165)

# Generate values for v
v_values = np.arange(2.7, 4.21, 0.01)  # 4.21 to include 4.2 with step 0.01

# Calculate corresponding f(v) values
f_values = f(v_values)

# Plotting
plt.figure(figsize=(10, 6))
plt.plot(v_values, f_values, label='f(v) = 100 / ((1 + (3.7 / v)^80)^0.165)')
plt.title('Plot of f(v)')
plt.xlabel('v')
plt.ylabel('f(v)')
plt.grid(True)
plt.legend()
plt.show()
