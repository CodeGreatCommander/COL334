import matplotlib.pyplot as plt

# Read data from the file
data = {}
with open("temp.txt", "r") as file:
    for line in file:
        parts = line.strip().split(", ")
        if len(parts) == 3:
            protocol, index, value = parts[0], int(parts[1]), float(parts[2])
            if protocol not in data:
                data[protocol] = []
            data[protocol].append((index, value))

# Separate data by protocol
fifo_data = data.get("fifo", [])
rr_data = data.get("rr", [])

# Create a single plot
plt.figure(figsize=(10, 5))

# Plot fifo data
if fifo_data:
    indices, values = zip(*fifo_data)
    plt.plot(indices, values, marker='o', linestyle='-', label='fifo')

# Plot rr data
if rr_data:
    indices, values = zip(*rr_data)
    plt.plot(indices, values, marker='s', linestyle='--', label='rr')


# Add labels and title
plt.xlabel('Number of Clients')
plt.ylabel('Time per client')
plt.title('Protocol Performance')
plt.legend()
plt.grid(True)

# Show the plot
plt.savefig('plot.png')