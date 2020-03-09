import math
import numpy as np
import scipy.stats

# all_rtts array
# <ueAddress,RTTarray> dict ue_rtts
# <ueAddress, int> dict ue_rtt_violations
# <ueAddress, int> dict ue_handovers
# <int, RTTarray> dict rtts_over_time
# <int,int> rtt_violations_over_time
# string[] handover_location_strings

experiment_prefix = "01"
rtt_threshold = 15
rtt_violations = 0
handovers = 0

def mean_confidence_interval(data, confidence=0.95):
    a = 1.0 * np.array(data)
    n = len(a)
    m, se = np.mean(a), scipy.stats.sem(a)
    h = se * scipy.stats.t.ppf((1 + confidence) / 2., n-1)
    return m, m-h, m+h

def process_ue_file:
    result_file = open("results" + experiment_prefix + "/UE.txt", "r")
    #TODO account for swapped args in UE file
    for line in result_file:
        args = line.split(',')
        if args[1] >= 320:
            if args[0] == "Delay":
                #Do delay processing stuff here
                time = args[1]
                time_block = floor(time)
                ue_name = args[2]
                rtt = int(args[5])
                
                all_rtts.append(rtt)
                rtt_array = rtts_over_time[time_block]
                rtt_array.append(rtt)
                rtts_over_time[time_block] = rtt_array
                        
                rtt_array = ue_rtts[ue_name]
                rtt_array.append(rtt)
                ue_rtts[ue_name] = rtt_array
                
                if rtt > rtt_threshold:
                    rtt_violations++
                    number_of_violations = ue_rtt_violations[ue_name]
                    number_of_violations++
                    ue_rtt_violations[ue_name] = number_of_violations   

                    violations_this_block = rtt_violations_over_time[time_block]
                    violations_this_block++
                    rtt_violations_over_time[time_block] = violations_this_block
                    
                    
            elif args[0] == "Handover":
                #Do handover processing stuff here
                ue_name = args[2]
                mec_from = args[3]
                mec_to = args[4]
                
                handovers++
                
                int ue_handover_count = ue_handovers[ue_name]
                ue_handover_count++
                ue_handovers[ue_name] = ue_handover_count
                
                #Write data to CSV file to make a scatterplot
                location = args[5]
                coordinates = location.split(':')
                handover_location_strings.append(coordinates[0] + "," + coordinates[1] + "\n")
 
def write_diagram_data_files():
    
    #Handover location scatterplot data
    handover_location_file = open("results" + experiment_prefix + "/handover_location.csv", "w")
    for location_string in handover_location_strings:
        file.write(location_string)

    #Mean RTT over time
    mean_rtt_over_time_file = open("results" + experiment_prefix + "/mean_rtt_over_time.csv", "w")
    for time_block in mean_rtt_over_time:
        mean_rtt = np.mean(mean_rtt_over_time[time_block])
        mean_rtt_over_time_file.write(time_block + "," + mean_rtt + "\n")
        
    #RTT violations over time
    rtt_violations_over_time_file = open("results" + experiment_prefix + "/rtt_violations_over_time.csv", "w")
    for time_block in rtt_violations_over_time:
        rtt_violations_over_time_file.write(time_block + "," + rtt_violations_over_time[time_block] + "\n")
        
    #Handovers over time
    handovers_over_time_file = open("results" + experiment_prefix + "/handovers_over_time.csv", "w")
    for time_block in handovers_over_time:
        handovers_over_time_file.write(time_block + "," + handovers_over_time[time_block] + "\n")

def print_results():
    #Do print formatting stuff here
    print "#############################################################################"
    print "Overall RTT confidence interval" + mean_confidence_interval(all_rtts)
    print "Individual RTT confidence intervals: "
    for ue in ue_rtts:
        print ue + ": " + mean_confidence_interval(ue_rtts[ue])
    print "#############################################################################"
    
    print "Total number of RTT violations: " + rtt_violations
    print "individual RTT violations: "
    for ue in ue_rtt_violations:
        print ue + ": " + ue_rtt_violations[ue]
    print "#############################################################################"

    print "Total number of handovers: " + handovers_over_time
    for ue in ue_handovers:
        print ue + ": " + ue_handovers[ue]
    print "#############################################################################"
    
    
def main():
    # Procedure for a single file; TODO extend for multiple files/runs
    process_ue_file()
    write_diagram_data_files()
    print_results()
