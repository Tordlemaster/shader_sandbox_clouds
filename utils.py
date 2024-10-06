from fractions import Fraction
from mido import MidiFile, Message
import itertools

circulusIntervals: list = [Fraction(2, 1), Fraction(3, 2), Fraction(5, 4), Fraction(4, 3), Fraction(5, 3), Fraction(6, 5), Fraction(8, 5)]
midiDistToRatioList: list = [Fraction(1, 1), None, None, Fraction(6, 5), Fraction(5, 4), Fraction(4, 3), None, Fraction(3, 2), Fraction(8, 5), Fraction(5, 3), None, None]
noteNames = ["C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"]

class Note: #Corresponding with a MIDI note
    def __init__(self, pitch: int, startTime) -> None:
        self.pitch: int = pitch
        self.startTime = startTime
    
    def __str__(self):
        return f"Note(pitch {self.pitch}, startTime {self.startTime})"
    
class Pitch: #An actual pitch
    def __init__(self, midiPitch: int, tuning: Fraction, tuningReferents: set[int]) -> None:
        self.midiPitch: int = midiPitch
        self.tuning: Fraction = tuning
        self.tuningReferents: set = tuningReferents #add all of these in the final pitch when adding to tunedChords
        self.pitchClass: str = noteNames[midiPitch % 12]
        self.octave: int = (midiPitch // 12) - 1
        pass

def makeWithinOneOctave(interval: Fraction) -> Fraction:
    while interval > 2:
        interval /= 2
    while interval <= 1:
        interval *= 2
    return interval

def isChordInTune(chord: list) -> bool:
    for i in range(len(chord)): #TEST ALL COMBINATIONS OF TWO NOTES IN THE CHORD
        for j in range(len(chord)):
            if i==j:
                continue
            else:
                interval = makeWithinOneOctave(chord[j]/chord[i])
                if interval not in circulusIntervals:
                    return False
    return True

def midiToNotes(file: MidiFile): #Notes described by start time since beginning
    notesTracks = []
    for track in file.tracks[1:]:
        notesTracks.append([])
        time = 0
        #If msg is note_on, add the corresponding note to notesTracks with startTime = time
        #Else add its time to time
        for msg in track: 
            if msg.type == "note_on":
                notesTracks[-1].append(Note(msg.note, time))
            else:
                time += msg.time
    return notesTracks

def midiDistToRatio(dist: int):
    if dist >= 0:
        ratioWithinOctave = midiDistToRatioList[dist % 12]
        octaves = dist // 12
    else:
        ratioWithinOctave = midiDistToRatioList[(dist) % -12]
        octaves = -(dist // -12)
    return ratioWithinOctave * (Fraction(2, 1) ** octaves)

def howMuchIsChordInTune(chord: list):
    numIntervals = 0
    numIntervalsInTune = 0
    for i in range(len(chord)): #TEST ALL COMBINATIONS OF TWO NOTES IN THE CHORD
        for j in range(len(chord)):
            if i==j:
                continue
            else:
                numIntervals += 1
                interval = makeWithinOneOctave(chord[j]/chord[i])
                if interval in circulusIntervals:
                    numIntervalsInTune += 1
    return numIntervalsInTune / numIntervals