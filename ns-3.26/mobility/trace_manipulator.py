import re

read_file = open("100Trace1.xml", "r")
write_file = open("100Trace1Edited.xml", "w")

offset = 9

for line in read_file:
    if "carflow" not in line:
        write_file.write(line)
    else:
        regex = 'id="carflow\.\d*\"'
        id_section = re.search(regex,line).group(0)
        
        regex2 = "\d+"
       	vehicle_number = re.search(regex2,id_section).group(0)
        
        new_id = (int(vehicle_number) + offset)
        new_line = re.sub("carflow\.\d+", str(new_id), line)
        write_file.write(new_line)
        
read_file.close()
write_file.close()
    
