# MPI-Langton-s-Ant-game
The purpose of this topic is to implement, in C, using the MPI library, a distributed and scalable program with the number of processes, aiming to simulate a modified version of Langton's Ant game - invented by Chris Langton.

##
Moreover, in order to fulfill one of the main goals of the subject, which is to implement distributed and scalable programs, we will consider multiple ants on the map. Thus, each element of the map can be occupied by at most one ant. This implies that there may be multiple ants on the same element at a given time.
###
So, given the rules of movement and the fact that a square has four sides, this implies that after each step of the simulation, there can be at most four ants on the same element simultaneously. In addition, the map has a fixed (limited) size, so an ant can leave the area, which will remove it from the game.


