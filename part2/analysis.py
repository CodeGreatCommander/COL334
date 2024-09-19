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
plt.plot(indices, values, marker='o', linestyle='-', color='b', label='Values')
plt.xlabel('Index')
plt.ylabel('Value')
plt.title('Values from temp.txt')
plt.legend()
plt.grid(True)
plt.savefig('plot.png')