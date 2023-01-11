import xml.etree.ElementTree as ET
import os  
import glob

tree = ET.parse("N64_game_list2.xml")
root = tree.getroot()

fileMatchList = []
hasMatchesCount = 0
notFoundCount = 0
renamedCount = 0
boxArtDirectory = "/home/kaili/Desktop/boxart"
# for game in root.iter("game"):
#     romName = game.attrib["name"]

#     romNameAlt = ""
#     archive = game.find("archive")
#     if archive is not None:
#         romNameAlt = archive.attrib["name"]

#     source = game.find("source")
#     if source is None:
#         continue

#     file = source.find("file")
#     if file is None:
#         continue
    
#     if "serial" not in file.attrib:
#         continue

#     romSerialNumber = file.attrib["serial"]
    
#     details = source.find("details")

#     if details is None:
#         continue

#     region = details.attrib["region"]

#     # rename file named <romName>.png
#     filename = romName + ".png"

#     boxartFile = os.path.join(boxArtDirectory, romName + ".png")
#     if os.path.isfile(boxartFile):
#         os.rename(boxartFile, os.path.join(boxArtDirectory, romSerialNumber + ".png"))
#         renamedCount += 1
#         # print("Renamed " + boxartFile + " to " + os.path.join(boxArtDirectory, romSerialNumber + ".png"))
#     else:
#         filenamePattern = os.path.join(boxArtDirectory, romNameAlt + "*.png")
#         filelist = glob.glob(filenamePattern)
#         if len(filelist) > 0:
#             hasMatchesCount += 1
#             print(filenamePattern + " has matches")
#             fileMatchList.append(filenamePattern)
#         foundMatch = False
#         for f in filelist:
#             if region in f:
#                 os.rename(f, os.path.join(boxArtDirectory, romSerialNumber + ".png"))
#                 foundMatch = True
#                 renamedCount += 1
#             if foundMatch:
#                 break
#         if not foundMatch:
#             for f in filelist:
#                 os.rename(f, os.path.join(boxArtDirectory, romSerialNumber + ".png"))
#                 foundMatch = True
#                 renamedCount += 1
#                 if foundMatch:
#                     break
            
#             if not foundMatch:
#                 # print("No matching files found")
#                 # print("No file named: " + boxartFile)
#                 notFoundCount += 1

#                 # nameWithoutRegion = romName.replace(" " + region, "")
#                 # parenSplit = romName.split(" (")
#                 # partialRomName = parenSplit[0] # should be the first part of the rom name
#                 # glob.glob()

            
# print("DONE! Renamed " + str(renamedCount) + " files. Skipped " + str(notFoundCount) + " files.")

# # if hasMatchesCount > 0:
# matchingGames = 0
# orphanedFiles = 0
# fixedFiles = 0
# files = os.listdir("/home/kaili/Desktop/boxart/")
# for file in files:
#     gamename = file.replace(".png", "")
#     if len(file) != 8:
#         foundAFile = False
#         # print(file)
#         orphanedFiles += 1
#         parts = gamename.split()
#         for game in root.iter("game"):
#             if gamename in game.attrib["name"]:
#                 matchingGames += 1
#                 foundAFile = True
#             if not foundAFile:
#                 if parts[0] in game.attrib["name"]:
#                     if game.find("archive") is not None:
#                         region = game.find("archive").attrib["region"]
#                         if region in gamename:
#                             matchingGames += 1
#                             foundAFile = True
#                             print(gamename)
#                             if game.find("source") is not None:
#                                 serial = ""
#                                 if game.find("source").find("archive") is not None:
#                                     serial = game.find("source").find("archive").attrib["serial"]
#                                 elif game.find("source").find("file") is not None:
#                                     serial = game.find("source").find("file").attrib.get("serial", "")
#                                 else:
#                                     print("No match for " + gamename)
                                
#                                 if serial != "":
#                                     os.rename(os.path.join("/home/kaili/Desktop/boxart", file), os.path.join("/home/kaili/Desktop/boxart", serial + ".png"))
#                                     fixedFiles += 1
                                
#             if foundAFile:
#                 break
#         if not foundAFile:
#             print("Can't fix " + gamename)

# print("Found " + str(matchingGames) + " remaining matching titles. " + str(len(files)) + " total files. " + str(orphanedFiles) + " orphaned files")
# print("Fixed " + str(fixedFiles) + " files")
