import math
import numpy as np
import scipy.stats
import copy

# Script to merge multiple runs of experiments

number_of_runs = 1


def calculate_confidence_interval(data, confidence=0.95):
    a = 1.0 * np.array(data)
    n = len(a)
    m, se = np.mean(a), scipy.stats.sem(a)
    h = se * scipy.stats.t.ppf((1 + confidence) / 2., n-1)
    # m is value, h is interval (half)width
    return m, h


# Merge summaries.csv
def merge_summaries():
    print("Merge summaries")
    summary_files = []
    for run_number in range(1, number_of_runs + 1):
        summary_files.append(open("results/" + str(run_number) + "summary.csv", "r"))

    # Read data from file
    rtt_vars = {}  # dictionary of strategy name, list of entries
    frequency_vars = {}
    count_vars = {}
    for metric in range(0, 2):
        for trigger in range(0, 4):
            strategy_name = str(metric) + str(trigger)
            rtt_vars[strategy_name] = []
            frequency_vars[strategy_name] = []
            count_vars[strategy_name] = []

    for file in summary_files:
        for line in file:
            args = line.split("/")
            logtype = args[0]

            if logtype == "rtt":
                rtt_vars["00"].append(float(args[1]))
                rtt_vars["01"].append(float(args[2]))
                rtt_vars["02"].append(float(args[3]))
                rtt_vars["03"].append(float(args[4]))
                rtt_vars["10"].append(float(args[5]))
                rtt_vars["11"].append(float(args[6]))
                rtt_vars["12"].append(float(args[7]))
                rtt_vars["13"].append(float(args[8]))
            elif logtype == "handover_frequency":
                frequency_vars["00"].append(float(args[1]))
                frequency_vars["01"].append(float(args[2]))
                frequency_vars["02"].append(float(args[3]))
                frequency_vars["03"].append(float(args[4]))
                frequency_vars["10"].append(float(args[5]))
                frequency_vars["11"].append(float(args[6]))
                frequency_vars["12"].append(float(args[7]))
                frequency_vars["13"].append(float(args[8]))
            elif logtype == "handover_count":
                count_vars["00"].append(int(args[1]))
                count_vars["01"].append(int(args[2]))
                count_vars["02"].append(int(args[3]))
                count_vars["03"].append(int(args[4]))
                count_vars["10"].append(int(args[5]))
                count_vars["11"].append(int(args[6]))
                count_vars["12"].append(int(args[7]))
                count_vars["13"].append(int(args[8]))
        file.close()

    # Calculate confidence intervals and write to file
    summary_output = open("results/merged_summary.csv", "w")

    rtt_string = "rtt/"
    for metric in range(0, 2):
        for trigger in range(0, 4):
            strategy = str(metric) + str(trigger)
            confidence_interval = calculate_confidence_interval(rtt_vars[strategy])
            rtt_string = rtt_string + str(confidence_interval[0]) + "/" + str(confidence_interval[1]) + "/"
    rtt_string = rtt_string[:-1] + "\n"
    summary_output.write(rtt_string)

    frequency_string = "handover_frequency/"
    for metric in range(0, 2):
        for trigger in range(0, 4):
            strategy = str(metric) + str(trigger)
            confidence_interval = calculate_confidence_interval(frequency_vars[strategy])
            frequency_string = frequency_string + str(confidence_interval[0]) + "/" + str(confidence_interval[1]) + "/"
    frequency_string = frequency_string[:-1] + "\n"
    summary_output.write(frequency_string)

    count_string = "frequency_count/"
    for metric in range(0, 2):
        for trigger in range(0, 4):
            strategy = str(metric) + str(trigger)
            confidence_interval = calculate_confidence_interval(count_vars[strategy])
            count_string = count_string + str(confidence_interval[0]) + "/" + str(confidence_interval[1]) + "/"
    count_string = count_string[:-1] + "\n"
    summary_output.write(count_string)

    summary_output.close()


# TODO merge clients.csv
def merge_clients():
    print("Merge clients")

    client_files = []
    for run_number in range(1, number_of_runs + 1):
        client_files.append(open("results/" + str(run_number) + "clients.csv", "r"))

    # Read data from files
    client_data = {}  # timeslot, dict(strat_name, [entries])
    strategy_dict = {}

    for metric in range(0, 2):
        for trigger in range(0, 4):
            for server in range(0, 3):
                entry = str(metric) + str(trigger) + str(server)
                strategy_dict[entry] = []

    for i in range(300, 1820):
        client_data[i] = copy.deepcopy(strategy_dict)

    for file in client_files:
        for line in file:
            args = line.split("/")
            time_slot = int(args[0])
            my_dict = client_data[time_slot]

            my_dict["000"].append(int(args[1]))
            my_dict["001"].append(int(args[2]))
            my_dict["002"].append(int(args[3]))
            my_dict["010"].append(int(args[4]))
            my_dict["011"].append(int(args[5]))
            my_dict["012"].append(int(args[6]))
            my_dict["020"].append(int(args[7]))
            my_dict["021"].append(int(args[8]))
            my_dict["022"].append(int(args[9]))
            my_dict["030"].append(int(args[10]))
            my_dict["031"].append(int(args[11]))
            my_dict["032"].append(int(args[12]))
            my_dict["100"].append(int(args[13]))
            my_dict["101"].append(int(args[14]))
            my_dict["102"].append(int(args[15]))
            my_dict["110"].append(int(args[16]))
            my_dict["111"].append(int(args[17]))
            my_dict["112"].append(int(args[18]))
            my_dict["120"].append(int(args[19]))
            my_dict["121"].append(int(args[20]))
            my_dict["122"].append(int(args[21]))
            my_dict["130"].append(int(args[22]))
            my_dict["131"].append(int(args[23]))
            my_dict["132"].append(int(args[24]))

            client_data[time_slot] = my_dict

        file.close()

    # Write mean data to file
    client_output_file = open("results/merged_clients.csv", "w")

    for time_slot in client_data:
        my_dict = client_data[time_slot]
        client_string = str(time_slot) + "/"

        for metric in range(0, 2):
            for trigger in range(0, 4):
                for server in range(0,3):
                    key = str(metric) + str(trigger) + str(server)
                    mean = float(sum(my_dict[key]))/len(my_dict[key])
                    client_string = client_string + str(mean) + "/"

        client_string = client_string[:-1] + "\n"
        client_output_file.write(client_string)

    client_output_file.close()


# TODO merge handovers.csv
def merge_handovers():
    print("Merge handovers")

    handover_files = []
    for run_number in range(1, number_of_runs + 1):
        handover_files.append(open("results/" + str(run_number) + "handovers.csv", "r"))

    handover_data = {}  # timeslot, dict(strat_name, [entries])
    strategy_dict = {}

    for metric in range(0, 2):
        for trigger in range(0, 4):
            strategy_name = str(metric) + str(trigger)
            strategy_dict[strategy_name] = []

    for i in range(300, 1820):
        handover_data[i] = copy.deepcopy(strategy_dict)

    for file in handover_files:
        for line in file:
            args = line.split("/")
            time_slot = int(args[0])
            my_dict = handover_data[time_slot]

            my_dict["00"].append(int(args[1]))
            my_dict["01"].append(int(args[2]))
            my_dict["02"].append(int(args[3]))
            my_dict["03"].append(int(args[4]))
            my_dict["10"].append(int(args[5]))
            my_dict["11"].append(int(args[6]))
            my_dict["12"].append(int(args[7]))
            my_dict["13"].append(int(args[8]))

            handover_data[time_slot] = my_dict
        file.close()

        # Write mean data to file
    handover_output_file = open("results/merged_handovers.csv", "w")

    for time_slot in handover_data:
        my_dict = handover_data[time_slot]
        handover_string = str(time_slot) + "/"

        for metric in range(0, 2):
            for trigger in range(0, 4):
                strategy = str(metric) + str(trigger)
                mean = float(sum(my_dict[strategy])) / len(my_dict[strategy])
                handover_string = handover_string + str(mean) + "/"

        handover_string = handover_string[:-1] + "\n"
        handover_output_file.write(handover_string)

    handover_output_file.close()


# TODO merge responsetimes.csv
def merge_responsetimes():
    print("Merge responsetimes")

    responsetime_files = []
    for run_number in range(1, number_of_runs + 1):
        responsetime_files.append(open("results/" + str(run_number) + "responsetime.csv", "r"))

    # Read data from files
    responsetime_data = {}  # timeslot, dict(strat_name, [entries])
    strategy_dict = {}
    for metric in range(0, 2):
        for trigger in range(0, 4):
            for server in range(0, 3):
                entry = str(metric) + str(trigger) + str(server)
                strategy_dict[entry] = []

    for i in range(300, 1820):
        responsetime_data[i] = copy.deepcopy(strategy_dict)

    for file in responsetime_files:
        for line in file:
            args = line.split("/")
            time_slot = int(args[0])
            my_dict = responsetime_data[time_slot]

            my_dict["000"].append(int(args[1]))
            my_dict["001"].append(int(args[2]))
            my_dict["002"].append(int(args[3]))
            my_dict["010"].append(int(args[4]))
            my_dict["011"].append(int(args[5]))
            my_dict["012"].append(int(args[6]))
            my_dict["020"].append(int(args[7]))
            my_dict["021"].append(int(args[8]))
            my_dict["022"].append(int(args[9]))
            my_dict["030"].append(int(args[10]))
            my_dict["031"].append(int(args[11]))
            my_dict["032"].append(int(args[12]))
            my_dict["100"].append(int(args[13]))
            my_dict["101"].append(int(args[14]))
            my_dict["102"].append(int(args[15]))
            my_dict["110"].append(int(args[16]))
            my_dict["111"].append(int(args[17]))
            my_dict["112"].append(int(args[18]))
            my_dict["120"].append(int(args[19]))
            my_dict["121"].append(int(args[20]))
            my_dict["122"].append(int(args[21]))
            my_dict["130"].append(int(args[22]))
            my_dict["131"].append(int(args[23]))
            my_dict["132"].append(int(args[24]))

            responsetime_data[time_slot] = my_dict

        file.close()

    # Write mean data to file
    responsetime_output_file = open("results/merged_responsetime.csv", "w")

    for time_slot in responsetime_data:
        my_dict = responsetime_data[time_slot]
        responsetime_string = str(time_slot) + "/"

        for metric in range(0, 2):
            for trigger in range(0, 4):
                for server in range(0, 3):
                    key = str(metric) + str(trigger) + str(server)
                    print("Key: " + key)
                    mean = float(sum(my_dict[key])) / len(my_dict[key])
                    responsetime_string = responsetime_string + str(mean) + "/"
        print(responsetime_string)

        responsetime_string = responsetime_string[:-1] + "\n"
        responsetime_output_file.write(responsetime_string)

    responsetime_output_file.close()


# TODO merge rtt.csv
def merge_rtts():
    print("Merge RTTs")

    rtt_files = []
    for run_number in range(1, number_of_runs + 1):
        rtt_files.append(open("results/" + str(run_number) + "rtt.csv", "r"))

    rtt_data = {}  # timeslot, dict(strat_name, [entries])
    strategy_dict = {}

    for metric in range(0, 2):
        for trigger in range(0, 4):
            strategy_name = str(metric) + str(trigger)
            strategy_dict[strategy_name] = []

    for i in range(300, 1820):
        rtt_data[i] = copy.deepcopy(strategy_dict)

    for file in rtt_files:
        for line in file:
            line = line.replace(",", ".")
            args = line.split("/")
            time_slot = int(args[0])
            my_dict = rtt_data[time_slot]

            my_dict["00"].append(float(args[1]))
            my_dict["01"].append(float(args[2]))
            my_dict["02"].append(float(args[3]))
            my_dict["03"].append(float(args[4]))
            my_dict["10"].append(float(args[5]))
            my_dict["11"].append(float(args[6]))
            my_dict["12"].append(float(args[7]))
            my_dict["13"].append(float(args[8]))

        file.close()

        # Write mean data to file
    rtt_output_file = open("results/merged_rtt.csv", "w")

    for time_slot in rtt_data:
        my_dict = rtt_data[time_slot]
        rtt_string = str(time_slot) + "/"

        for metric in range(0, 2):
            for trigger in range(0, 4):
                strategy = str(metric) + str(trigger)
                mean = float(sum(my_dict[strategy])) / len(my_dict[strategy])
                rtt_string = rtt_string + str(mean) + "/"

        rtt_string = rtt_string[:-1] + "\n"
        rtt_output_file.write(rtt_string)

    rtt_output_file.close()


def main():
    merge_summaries()
    merge_clients()
    merge_handovers()
    merge_responsetimes()
    merge_rtts()


main()
