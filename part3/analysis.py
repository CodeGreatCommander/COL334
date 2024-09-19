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
aloha_data = data.get("aloha", [])
beb_data = data.get("beb", [])
csma_data = data.get("csma", [])

# Create a single plot
plt.figure(figsize=(10, 5))

# Plot ALOHA data
if aloha_data:
    indices, values = zip(*aloha_data)
    plt.plot(indices, values, marker='o', linestyle='-', label='ALOHA')

# Plot BEB data
if beb_data:
    indices, values = zip(*beb_data)
    plt.plot(indices, values, marker='s', linestyle='--', label='BEB')

# Plot CSMA data
if csma_data:
    indices, values = zip(*csma_data)
    plt.plot(indices, values, marker='^', linestyle='-.', label='CSMA')

# Add labels and title
plt.xlabel('Index')
plt.ylabel('Value')
plt.title('Protocol Performance')
plt.legend()
plt.grid(True)

# Show the plot
plt.savefig('plot.png')