import math
import numpy as np
import scipy.stats

####################################
# Data processing for a single run #
####################################

# UE data files
file00ue = open("results00/1UE.txt", "r")
file01ue = open("results01/1UE.txt", "r")
file02ue = open("results02/1UE.txt", "r")
file03ue = open("results03/1UE.txt", "r")
# file10ue = open("results10/1UE.txt", "r")
# file11ue = open("results11/1UE.txt", "r")
# file12ue = open("results12/1UE.txt", "r")
# file13ue = open("results13/1UE.txt", "r")
ue_files = [file00ue, file01ue, file02ue, file03ue]
    # , file10ue, file11ue, file12ue, file13ue]

# MEC data files
file00mec = open("results00/1MEC.txt", "r")
file01mec = open("results01/1MEC.txt", "r")
file02mec = open("results02/1MEC.txt", "r")
file03mec = open("results03/1MEC.txt", "r")
# file10mec = open("results10/1MEC.txt", "r")
# file11mec = open("results11/1MEC.txt", "r")
# file12mec = open("results12/1MEC.txt", "r")
# file13mec = open("results13/1MEC.txt", "r")
mec_files = [file00mec, file01mec, file02mec, file03mec]
    # , file10eu, file11mec, file12mec, file13mec]

# RTT dictionaries (timeslot, [totalDelays, numberOfDelays, mean])
rtt00 = {}
rtt01 = {}
rtt02 = {}
rtt03 = {}
# rtt10 = {}
# rtt11 = {}
# rtt12 = {}
# rtt13 = {}
rtt_dictionaries = [rtt00, rtt01, rtt02, rtt03]
    # , rtt10, rtt11, rtt12, rtt13]

# Handover dictionaries (times_lot, [totalHandovers, count, mean])
handover00 = {}
handover01 = {}
handover02 = {}
handover03 = {}
# handover10 = {}
# handover11 = {}
# handover12 = {}
# handover13 = {}
handover_dictionaries = [handover00, handover01, handover02, handover03]
    # , handover10, handover11, handover12, handover13]

# Clients per MEC dictionaries (time_slot, [#clientsmec1, #clientsmec2, #clientsmec3])
clients00 = {}
clients01 = {}
clients02 = {}
clients03 = {}
# clients10 = {}
# clients11 = {}
# clients12 = {}
# clients13 = {}
client_dictionaries = [clients00, clients01, clients02, clients03]
    # , clients10, clients11, clients12, clients13]

# Response time per MEC dictionaries (time_slot, [response_mec1, response_mec2, response_mec3])
response00 = {}
response01 = {}
response02 = {}
response03 = {}
# response10 = {}
# response11 = {}
# response12 = {}
# response13 = {}
response_dictionaries = [response00, response01, response02, response03]
    # , response10, response11, response12, response13]

mean_rtt_per_strategy = {}  # filename, mean_rtt
handover_frequency_per_strategy = {}  # filename, mean_handover_freq
handover_count_per_strategy = {}  # filename, count

# Process UE files into variables
for file in ue_files:
    experiment_index = ue_files.index(file)
    rtt_stats = [0, 0, 0]
    handover_stats = [0, 0, 0]
    for line in file:
            args = line.split('/')
            logtype = args[0]

            if logtype == "delay":
                source = args[1]
                time_slot = int(args[2])

                my_mean_rtt = int(args[3])
                my_rtt_dictionary = rtt_dictionaries[experiment_index]

                new_sum = (rtt_stats[0] + my_mean_rtt)
                new_count = (rtt_stats[1] + 1)
                rtt_stats = [new_sum, new_count, float(new_sum)/float(new_count)]

                if time_slot in my_rtt_dictionary:
                    mean_list = my_rtt_dictionary[time_slot]
                    new_total_delays = mean_list[0] + my_mean_rtt
                    new_number_delays = mean_list[1] + 1
                    new_mean = float(new_total_delays)/float(new_number_delays)

                    new_mean_list = [new_total_delays, new_number_delays, new_mean]
                    my_rtt_dictionary[time_slot] = new_mean_list

                else:
                    my_rtt_dictionary[time_slot] = [my_mean_rtt, 1, my_mean_rtt]
                    rtt_stats = [my_mean_rtt, 1, my_mean_rtt]

            elif logtype == "handovers":
                source = args[1]
                time_slot = int(args[2])

                my_handover_dictionary = handover_dictionaries[experiment_index]
                my_handovers = int(args[3])
                if time_slot in my_handover_dictionary:
                    mean_list = my_handover_dictionary[time_slot]
                    new_total_handovers = mean_list[0] + my_handovers
                    new_count = mean_list[1] + 1
                    new_mean = float(new_total_handovers)/float(new_count)

                    new_mean_list = [new_total_handovers, new_count, new_mean]
                    my_handover_dictionary[time_slot] = new_mean_list

                    new_globaltotal = handover_stats[0] + my_handovers
                    new_globalcount = handover_stats[1] + 1
                    handover_stats = [new_globaltotal, new_globalcount, float(new_globaltotal)/float(new_globalcount)]
                else:
                    my_handover_dictionary[time_slot] = [my_handovers, 1, my_handovers]
                    handover_stats = [my_handovers, 1, my_handovers]
                
    mean_rtt_per_strategy[file.name] = rtt_stats[2]
    handover_frequency_per_strategy[file.name] = handover_stats[2]
    handover_count_per_strategy[file.name] = handover_stats[0]
    file.close()


# Process MEC files into variables
for file in mec_files:
    experiment_index = mec_files.index(file)
    for line in file:
        args = line.split('/')
        if args[0].startswith("Server"):            # This is a data line, not a sanity check or queuecounter line
            time_string = args[0].split(',')[1]
            time_slot = math.floor(float(time_string[1:]))
            mec_id = args[1]
            clients = int(args[3])
            response_time = int(args[4])

            # Add MEC client numbers to variables
            my_client_dictionary = client_dictionaries[experiment_index]
            if time_slot in my_client_dictionary:
                client_values = my_client_dictionary[time_slot]
                if mec_id == "1.0.0.3":
                    new_client_values = [clients, client_values[1], client_values[2]]
                    my_client_dictionary[time_slot] = new_client_values
                elif mec_id == "1.0.0.5":
                    new_client_values = [client_values[0], clients, client_values[2]]
                    my_client_dictionary[time_slot] = new_client_values
                elif mec_id == "1.0.0.7":
                    new_client_values = [client_values[0], client_values[1], clients]
                    my_client_dictionary[time_slot] = new_client_values
            else:
                if mec_id == "1.0.0.3":
                    my_client_dictionary[time_slot] = [clients, -1, -1]
                elif mec_id == "1.0.0.5":
                    my_client_dictionary[time_slot] = [-1, clients, -1]
                elif mec_id == "1.0.0.7":
                    my_client_dictionary[time_slot] = [-1, -1, clients]

            # Add MEC response time numbers to variables
            my_response_dictionary = response_dictionaries[experiment_index]
            if time_slot in my_response_dictionary:
                response_values = my_response_dictionary[time_slot]
                if mec_id == "1.0.0.3":
                    new_response_values = [response_time, response_values[1], response_values[2]]
                    my_response_dictionary[time_slot] = new_response_values
                elif mec_id == "1.0.0.5":
                    new_response_values = [response_values[0], response_time, response_values[2]]
                    my_response_dictionary[time_slot] = new_response_values
                elif mec_id == "1.0.0.7":
                    new_response_values = [response_values[0], response_values[1], response_time]
                    my_response_dictionary[time_slot] = new_response_values
            else:
                if mec_id == "1.0.0.3":
                    my_response_dictionary[time_slot] = [response_time, -1, -1]
                elif mec_id == "1.0.0.5":
                    my_response_dictionary[time_slot] = [-1, response_time, -1]
                elif mec_id == "1.0.0.7":
                    my_response_dictionary[time_slot] = [-1, -1, response_time]
    file.close()

# Write data file for RTT graph (time_slot, rtt00, rtt01 etc)
rtt_output_file = open("results/rtt.csv", "w")
for i in range(300, 1820):
    file_string = str(i) + "/"
    for rtt_dictionary in rtt_dictionaries:
        mean_rtt = rtt_dictionary[i][2]
        file_string = file_string + str(mean_rtt) + "/"
    file_string = file_string[:-1] + "\n"
    file_string = file_string.replace('.', ',')
    rtt_output_file.write(file_string)
rtt_output_file.close()

# Write data file for handover frequency graph
handover_output_file = open("results/handovers.csv", "w")
for i in range(300, 1820):
    file_string = str(i) + "/"
    for handover_dictionary in handover_dictionaries:
        if i in handover_dictionary:
            mean_handovers = handover_dictionary[i][2]
            file_string = file_string + str(mean_handovers) + "/"
        else:
            file_string = file_string + "0/"
    file_string = file_string[:-1] + "\n"
    file_string = file_string.replace('.', ',')
    handover_output_file.write(file_string)
handover_output_file.close()

# Write data file for clients per MEC graph
clients_output_file = open("results/clients.csv", "w")
for i in range(300, 1820):
    file_string = str(i) + "/"
    for client_dictionary in client_dictionaries:
        if i in client_dictionary:
            client_entries = client_dictionary[i]
            for j in range(0, 3):
                if client_entries[j] is -1:
                    file_string = file_string + "/"
                else:
                    file_string = file_string + str(client_entries[j]) + "/"
        else:
            continue
    file_string = file_string[:-1] + "\n"
    file_string = file_string.replace('.', ',')
    clients_output_file.write(file_string)
clients_output_file.close()

# Write data file for response times per MEC graph
response_output_file = open("results/responsetime.csv", "w")
for i in range(300, 1820):
    file_string = str(i) + "/"
    for response_dictionary in response_dictionaries:
        if i in response_dictionary:
            response_list = response_dictionary[i]
            file_string = file_string + str(response_list[0]) + "/" + str(response_list[1]) + "/" + str(response_list[2]) + "/"
        else:
            continue
    file_string = file_string[:-1] + "\n"
    file_string = file_string.replace('.', ',')
    response_output_file.write(file_string)
response_output_file.close()

# Print some statistics per strategy: mean RTT, mean handover frequency, total number of handovers
print("Mean RTT per strategy")
for name in mean_rtt_per_strategy:
    print(name + ": " + str(mean_rtt_per_strategy[name]) + "\n")

print("Mean handover frequency per strategy")
for name in handover_frequency_per_strategy:
    print(name + ": " + str(handover_frequency_per_strategy[name]) + "\n")

print("Number of handovers per strategy")
for name in handover_count_per_strategy:
    print(name + ": " + str(handover_count_per_strategy[name]) + "\n")
