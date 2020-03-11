import math
import numpy as np
import scipy.stats
import matplotlib.pyplot as plt


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


# for i in range(320, 1821):
#     rtt_violations_over_time[i] = 0
#     handovers_over_time[i] = 0
#
# # Process UE file
# result_file = open("results" + experiment_prefix + "/UE.txt", "r")
#
# for line in result_file:
#     args = line.split(',')
#     if float(args[1]) >= 320:
#         if args[0] == "Delay":
#             # Do delay processing stuff here
#             time = args[1]
#             time_block = int(math.floor(float(time)))
#             ue_name = args[2]
#             rtt = int(args[5])
#
#             # Add 0-entries if ue_name is not in ue_rtt_violations, ue_handovers
#             if ue_rtt_violations.get(ue_name) is None:
#                 ue_rtt_violations[ue_name] = 0
#             if ue_handovers.get(ue_name) is None:
#                 ue_handovers[ue_name] = 0
#
#             all_rtts.append(rtt)
#             if rtts_over_time.get(time_block) is not None:
#                 rtt_array = rtts_over_time[time_block]
#                 rtt_array.append(rtt)
#                 rtts_over_time[time_block] = rtt_array
#             else:
#                 rtts_over_time[time_block] = [rtt]
#
#             if ue_rtts.get(ue_name) is not None:
#                 rtt_array = ue_rtts[ue_name]
#                 rtt_array.append(rtt)
#                 ue_rtts[ue_name] = rtt_array
#             else:
#                 ue_rtts[ue_name] = [rtt]
#
#             if rtt > rtt_threshold:
#                 rtt_violations += 1
#                 if ue_rtt_violations.get(ue_name) is not None:
#                     number_of_violations = ue_rtt_violations[ue_name]
#                     number_of_violations += 1
#                     ue_rtt_violations[ue_name] = number_of_violations
#                 else:
#                     ue_rtt_violations[ue_name] = 1
#
#                 if rtt_violations_over_time.get(time_block) is not None:
#                     violations_this_block = rtt_violations_over_time[time_block]
#                     violations_this_block += 1
#                     rtt_violations_over_time[time_block] = violations_this_block
#                 else:
#                     rtt_violations_over_time[time_block] = 1
#
#         elif args[0] == "Handover":
#             # Do handover processing stuff here
#             time_block = int(math.floor(float(args[1])))
#             ue_name = args[2]
#             mec_from = args[3]
#             mec_to = args[4]
#
#             # Add 0-entries if ue_name is not in ue_rtt_violations, ue_handovers
#             if ue_rtt_violations.get(ue_name) is None:
#                 ue_rtt_violations[ue_name] = 0
#             if ue_handovers.get(ue_name) is None:
#                 ue_handovers[ue_name] = 0
#
#             handovers += 1
#
#             if ue_handovers.get(ue_name) is not None:
#                 ue_handover_count = ue_handovers[ue_name]
#                 ue_handover_count += 1
#                 ue_handovers[ue_name] = ue_handover_count
#             else:
#                 ue_handovers[ue_name] = 1
#
#             # Log handovers by time block
#             if handovers_over_time.get(time_block) is not None:
#                 handovers_in_block = handovers_over_time[time_block]
#                 handovers_in_block += 1
#                 handovers_over_time[time_block] = handovers_in_block
#             else:
#                 handovers_over_time[time_block] = 1
#
#             # Write data to CSV file to make a scatterplot
#             location = args[5]
#             coordinates = location.split(':')
#             handover_location_strings.append(coordinates[0] + "," + coordinates[1] + "\n")


# Process MEC file
mec_result_file = open("results" + experiment_prefix + "/MEC.txt", "r")
mec_clients_file = open("results" + experiment_prefix + "/mec_clients.csv", "w")
mec_response_file = open("results" + experiment_prefix + "/mec_response.csv", "w")

times = []
mec1_clients = []
mec2_clients = []
mec3_clients = []
mec1_response = []
mec2_response = []
mec3_response = []

for line in mec_result_file:
    args = line.split(',')
    if args[0] == "Server response time":

        address = args[2]
        no_clients = args[3]
        response_time = args[4][:-1]

        if address == " 1.0.0.3":
            times.append(args[1])
            mec1_clients.append(no_clients)
            mec1_response.append(response_time)
        elif address == " 1.0.0.5":
            mec2_clients.append(no_clients)
            mec2_response.append(response_time)
        elif address == " 1.0.0.7":
            mec3_clients.append(no_clients)
            mec3_response.append(response_time)

for i in range(len(times)):
    mec_clients_file.write(times[i] + "," + mec1_clients[i] + "," + mec2_clients[i] + "," + mec3_clients[i] + "\n")
    mec_response_file.write(times[i] + "," + mec1_response[i] + "," + mec2_response[i] + "," + mec3_response[i] + "\n")

# Handover location scatterplot data
# handover_location_file = open("results" + experiment_prefix + "/handover_location.csv", "w")
# for location_string in handover_location_strings:
#     handover_location_file.write(location_string)
#
# # Mean RTT over time
# mean_rtt_over_time_file = open("results" + experiment_prefix + "/mean_rtt_over_time.csv", "w")
# for time_block in rtts_over_time:
#     mean_rtt = np.mean(rtts_over_time[time_block])
#     mean_rtt_over_time_file.write(str(time_block) + "," + str(mean_rtt) + "\n")
#
# # RTT violations over time
# rtt_violations_over_time_file = open("results" + experiment_prefix + "/rtt_violations_over_time.csv", "w")
#
# for time_block in rtt_violations_over_time:
#     rtt_violations_over_time_file.write(str(time_block) + "," + str(rtt_violations_over_time[time_block]) + "\n")
#
# # Handovers over time
# handovers_over_time_file = open("results" + experiment_prefix + "/handovers_over_time.csv", "w")
# for time_block in handovers_over_time:
#     handovers_over_time_file.write(str(time_block) + "," + str(handovers_over_time[time_block]) + "\n")
#
# # Do print(formatting stuff here
# print("Overall RTT confidence interval: " + str(mean_confidence_interval(all_rtts)))
# min_mean = 10000
# min_tuple = ()
# min_ue = ""
# max_mean = -1
# max_tuple = ()
# max_ue = ""
#
# for ue in ue_rtts:
#     ci_tuple = mean_confidence_interval(ue_rtts[ue])
#     if ci_tuple[0] < min_mean:
#         min_mean = ci_tuple[0]
#         min_tuple = ci_tuple
#         min_ue = ue
#     if ci_tuple[0] > max_mean:
#         max_mean = ci_tuple[0]
#         max_tuple = ci_tuple
#         max_ue = ue
# print("Minimum mean: " + str(min_tuple) + " for UE " + min_ue)
# print("Maximum mean: " + str(max_tuple) + " for UE " + max_ue)
# print("\n")
#
# print("Total number of RTT violations: " + str(rtt_violations))
# ue_violation_values = ue_rtt_violations.values()
# print("Mean number of violations per UE: " + str(mean_confidence_interval(ue_violation_values)))
# print("Minimum number of violations per UE: " + str(min(ue_violation_values)))
# print("Maximum number of violations per UE: " + str(max(ue_violation_values)))
# # print("individual RTT violations: ")
# # for ue in ue_rtt_violations:
# #     print(ue + ": " + str(ue_rtt_violations[ue]))
# print("\n")
#
# print("Total number of handovers: " + str(handovers))
# ue_handover_values = ue_handovers.values()
# print("Mean number of handovers per UE: " + str(mean_confidence_interval(ue_handover_values)))
# print("Minimum number of handovers per UE: " + str(min(ue_handover_values)))
# print("Maximum number of handovers per UE: " + str(max(ue_handover_values)))
# # for ue in ue_handovers:
# #     print(ue + ": " + str(ue_handovers[ue]))
#
# print("\n")

mec_result_file.close()
mec_clients_file.close()
mec_response_file.close()