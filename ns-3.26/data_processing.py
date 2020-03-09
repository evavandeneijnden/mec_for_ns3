import math
import numpy as np
import scipy.stats


all_rtts = []                       # int[]
ue_rtts = {}                        # <ueAddress,RTTarray>
ue_rtt_violations = {}              # <ueAddress, int>
ue_handovers = {}                   # <ueAddress, int>
rtts_over_time = {}                 # <int, RTTarray>
rtt_violations_over_time = {}       # <int,int>
handover_location_strings = []      # string[]
handovers_over_time = {}            # <int,int>

experiment_prefix = "13"
rtt_threshold = 15
rtt_violations = 0
handovers = 0


def mean_confidence_interval(data, confidence=0.95):
    a = 1.0 * np.array(data)
    n = len(a)
    m, se = np.mean(a), scipy.stats.sem(a)
    h = se * scipy.stats.t.ppf((1 + confidence) / 2., n-1)
    return m, m-h, m+h


result_file = open("results" + experiment_prefix + "/UE.txt", "r")

# Make 0-entries for rtt_violations_over_time and handovers_over_time
lines = result_file.read().splitlines()
last_line = lines[-1]
last_args = last_line.split(',')
last_time_block = int(math.floor(float(last_args[1])))

for i in range(320, 1821):
    rtt_violations_over_time[i] = 0
    handovers_over_time[i] = 0

result_file.close()

print("Process UE file")
result_file = open("results" + experiment_prefix + "/UE.txt", "r")

for line in result_file:
    args = line.split(',')
    if float(args[1]) >= 320:
        if args[0] == "Delay":
            # Do delay processing stuff here
            time = args[1]
            time_block = int(math.floor(float(time)))
            ue_name = args[2]
            rtt = int(args[5])

            # print("time block type: " + str(type(time_block)))

            # Add 0-entries if ue_name is not in ue_rtt_violations, ue_handovers
            if ue_rtt_violations.get(ue_name) is None:
                ue_rtt_violations[ue_name] = 0
            if ue_handovers.get(ue_name) is None:
                ue_handovers[ue_name] = 0

            all_rtts.append(rtt)
            if rtts_over_time.get(time_block) is not None:
                rtt_array = rtts_over_time[time_block]
                rtt_array.append(rtt)
                rtts_over_time[time_block] = rtt_array
            else:
                rtts_over_time[time_block] = [rtt]

            if ue_rtts.get(ue_name) is not None:
                rtt_array = ue_rtts[ue_name]
                rtt_array.append(rtt)
                ue_rtts[ue_name] = rtt_array
            else:
                ue_rtts[ue_name] = [rtt]

            if rtt > rtt_threshold:
                rtt_violations += 1
                if ue_rtt_violations.get(ue_name) is not None:
                    number_of_violations = ue_rtt_violations[ue_name]
                    number_of_violations += 1
                    ue_rtt_violations[ue_name] = number_of_violations
                else:
                    ue_rtt_violations[ue_name] = 1

                if rtt_violations_over_time.get(time_block) is not None:
                    violations_this_block = rtt_violations_over_time[time_block]
                    violations_this_block += 1
                    rtt_violations_over_time[time_block] = violations_this_block
                else:
                    rtt_violations_over_time[time_block] = 1

        elif args[0] == "Handover":
            # Do handover processing stuff here
            time_block = int(math.floor(float(args[1])))
            ue_name = args[2]
            mec_from = args[3]
            mec_to = args[4]

            # Add 0-entries if ue_name is not in ue_rtt_violations, ue_handovers
            if ue_rtt_violations.get(ue_name) is None:
                ue_rtt_violations[ue_name] = 0
            if ue_handovers.get(ue_name) is None:
                ue_handovers[ue_name] = 0

            handovers += 1

            if ue_handovers.get(ue_name) is not None:
                ue_handover_count = ue_handovers[ue_name]
                ue_handover_count += 1
                ue_handovers[ue_name] = ue_handover_count
            else:
                ue_handovers[ue_name] = 1

            # Log handovers by time block
            if handovers_over_time.get(time_block) is not None:
                handovers_in_block = handovers_over_time[time_block]
                handovers_in_block += 1
                handovers_over_time[time_block] = handovers_in_block
            else:
                handovers_over_time[time_block] = 1

            # Write data to CSV file to make a scatterplot
            location = args[5]
            coordinates = location.split(':')
            handover_location_strings.append(coordinates[0] + "," + coordinates[1] + "\n")

print("Write diagram files")
# Handover location scatterplot data
handover_location_file = open("results" + experiment_prefix + "/handover_location.csv", "w")
for location_string in handover_location_strings:
    handover_location_file.write(location_string)

# Mean RTT over time
mean_rtt_over_time_file = open("results" + experiment_prefix + "/mean_rtt_over_time.csv", "w")
for time_block in rtts_over_time:
    mean_rtt = np.mean(rtts_over_time[time_block])
    mean_rtt_over_time_file.write(str(time_block) + "," + str(mean_rtt) + "\n")

# RTT violations over time
rtt_violations_over_time_file = open("results" + experiment_prefix + "/rtt_violations_over_time.csv", "w")

for time_block in rtt_violations_over_time:
    # print(str(time_block) + ", " + str(rtt_violations_over_time[time_block]))
    rtt_violations_over_time_file.write(str(time_block) + "," + str(rtt_violations_over_time[time_block]) + "\n")

# Handovers over time
handovers_over_time_file = open("results" + experiment_prefix + "/handovers_over_time.csv", "w")
for time_block in handovers_over_time:
    handovers_over_time_file.write(str(time_block) + "," + str(handovers_over_time[time_block]) + "\n")

# Do print(formatting stuff here
print("#############################################################################\n")
print("Overall RTT confidence interval" + str(mean_confidence_interval(all_rtts)))
print("Individual RTT confidence intervals: ")
for ue in ue_rtts:
    print(ue + ": " + str(mean_confidence_interval(ue_rtts[ue])))
print("#############################################################################\n")

print("Total number of RTT violations: " + str(rtt_violations))
print("individual RTT violations: ")
for ue in ue_rtt_violations:
    print(ue + ": " + str(ue_rtt_violations[ue]))
print("#############################################################################\n")

print("Total number of handovers: " + str(handovers))
for ue in ue_handovers:
    print(ue + ": " + str(ue_handovers[ue]))
print("#############################################################################\n")
