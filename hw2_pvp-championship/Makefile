all: battle player 

battle: battle.c 
	gcc $^ log.c graph.c -fsanitize=undefined -o $@	

player: player.c 
	gcc $^ log.c graph.c -fsanitize=undefined -o $@	
clean:
	rm -f battle player log_* *.fifo 

