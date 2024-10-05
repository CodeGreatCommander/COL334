import matplotlib.pyplot as plt
import numpy as np
import scipy.stats as stats

# Read data from the file
with open("temp.txt", "r") as file:
    data = [float(line.strip()) for line in file]

# Calculate means for consecutive 10 values
means = [np.mean(data[i:i+10]) for i in range(0, len(data), 10)]

# Calculate confidence intervals
confidence_intervals = []
for i in range(0, len(data), 10):
    segment = data[i:i+10]
    if len(segment) == 10:
        mean = np.mean(segment)
        std_err = stats.sem(segment)
        h = std_err * stats.t.ppf((1 + 0.95) / 2, len(segment) - 1)
        confidence_intervals.append((mean - h, mean + h))

# Plot the means and confidence intervals
x = range(1,len(means)+1)
means = np.array(means)
conf_intervals = np.array(confidence_intervals)

plt.figure(figsize=(10, 5))
plt.plot(x, means, label='Mean')
plt.fill_between(x, conf_intervals[:, 0], conf_intervals[:, 1], color='b', alpha=0.2, label='95% Confidence Interval')
plt.xlabel('Segment')
plt.ylabel('Mean Time(microseconds)')
plt.title('Mean Time and Confidence Interval for Different P')
plt.legend()
plt.savefig("plot.png")