from fractions import Fraction
from mido import MidiFile
import itertools
from utils import *

music: MidiFile = MidiFile("benedetti twice.mid")
numTracks = len(music.tracks) - 1 #TODO Does not apply to all MIDI files

times: list = [0] * (numTracks) #current time in each track

#Every time a set of notes change in one moment a copy of the entire current harmony is added to this
#Indexed [chord index][track index] unlike other lists
tunedChords: list[list[Pitch]] = [] 

#All the notes that move at once must find referents in the previous set of notes such that they are most in tune with each other

noteTracks: list = midiToNotes(music)
noteTrackLengths: list = [len(t) for t in noteTracks]
curTrackPositions: list = [0] * (numTracks) #The positions of the next set of notes up for tuning

#While not all tracks have reached the end:
#Find the soonest-to-come next notes
#Tune them based on most recent chord in tunedChords:
#If they are repeated notes, keep tuning from previous chord

while sum([(noteTrackLengths[x] >= curTrackPositions[x]) for x in range(numTracks)]) < (numTracks):
    nextNotesTime = min([noteTracks[i][curTrackPositions[i]].startTime for i in range(numTracks)])
    
    tracksWithNextNotes = [] #Indices of tracks with notes that are moving now
    for i in range(len(music.notes) - 1):
        if noteTracks[i][curTrackPositions[i]].startTime == nextNotesTime:
            tracksWithNextNotes.append(i)

    possiblePitchesForEachTrack: list[list[Pitch]] = []#Possible tunings of the next pitches for each track
    for i in range(numTracks): #Add next note tuning possibilities
        possiblePitchesForEachTrack.append([])
        if i in tracksWithNextNotes: #If this track has a note moving now
            nextNote: Note = noteTracks[i][curTrackPositions[i]]
            for j in range(numTracks): #Look at all notes in previous chord as possible tuning referents
                intervalDist = nextNote.pitch - tunedChords[-1][j].midiPitch
                intervalRatio = midiDistToRatio(intervalDist)
                newPitch = tunedChords[-1][j].tuning * intervalRatio
                possiblePitchesForEachTrack[i].append(Pitch(nextNote, newPitch, set([j])))
        else:
            #If pitch in track i is not changing,copy pitch from previous chord
            possiblePitchesForEachTrack[i].append(tunedChords[-1][i])
    
    possibleChordTunings = [] #All possible next chord tunings resulting from combinations of choices of tuning referents
    numPossibleChords: int = 1
    for track in possiblePitchesForEachTrack:
        numPossibleChords *= len(track)
    possibleChordTunings: list[list[Pitch]] = itertools.product(*possiblePitchesForEachTrack)
    assert(len(possibleChordTunings) == numPossibleChords)

    maximumTuningProportion = max([howMuchIsChordInTune(c) for c in possibleChordTunings])
    assert(maximumTuningProportion <= 1)
    chordsMaxInTune: list[list[Pitch]] = []
    for c in possibleChordTunings:
        if howMuchIsChordInTune(c) == maximumTuningProportion:
            chordsMaxInTune.append(c)
    
    #Assert that all maximally tuned chords have the same tuning ratios (probably not true)
    #TODO add a score of how non-Cartesian each interval is in howMuchIsChordInTune

    newChord = chordsMaxInTune[0]
    for c in chordsMaxInTune:
        assert(len(c) == numTracks)
        for t in range(len(c)): #Tracks
            newChord[t].tuningReferents.add(list(c[t].tuningReferents))
    
    tunedChords.append(newChord)
    for t in tracksWithNextNotes:
        curTrackPositions[t] += 1

for c in tunedChords:
    print([x.tuning for x in c])