import matplotlib.pyplot as plt

# Read data from the file
data = []
with open("temp.txt", "r") as file:
    for line in file:
        parts = line.strip().split(", ")
        if len(parts) == 2:
            index, value = int(parts[0]), float(parts[1])
            data.append((index, value))

# Extract indices and values
indices = [item[0] for item in data]
values = [item[1] for item in data]

# Plot the data
plt.figure(figsize=(10, 5))
plt.plot(indices, values, marker='o', linestyle='-', color='b', label='Time')
plt.xlabel('No of Clients')
plt.ylabel('Average Time (s)')
plt.title('Performance Analysis')
plt.legend()
plt.grid(True)
plt.savefig('plot.png')