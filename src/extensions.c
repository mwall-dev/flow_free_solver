#include "extensions.h"
#include "options.h"

//////////////////////////////////////////////////////////////////////
// For sorting colors

int color_features_compare(const void* vptr_a, const void* vptr_b) {

	const color_features_t* a = (const color_features_t*)vptr_a;
	const color_features_t* b = (const color_features_t*)vptr_b;

	int u = cmp(a->user_index, b->user_index);
	if (u) { return u; }

	int w = cmp(a->wall_dist[0], b->wall_dist[0]);
	if (w) { return w; }

	int g = -cmp(a->wall_dist[1], b->wall_dist[1]);
	if (g) { return g; }

	return -cmp(a->min_dist, b->min_dist);

}

//////////////////////////////////////////////////////////////////////
// Place the game colors into a set order

void game_order_colors(game_info_t* info,
                       game_state_t* state) {

	if (g_options.order_random) {
    
		srand(now() * 1e6);
    
		for (size_t i=info->num_colors-1; i>0; --i) {
			size_t j = rand() % (i+1);
			int tmp = info->color_order[i];
			info->color_order[i] = info->color_order[j];
			info->color_order[j] = tmp;
		}

	} else { // not random

		color_features_t cf[MAX_COLORS];
		memset(cf, 0, sizeof(cf));

		for (size_t color=0; color<info->num_colors; ++color) {
			cf[color].index = color;
			cf[color].user_index = MAX_COLORS;
		}
    

		for (size_t color=0; color<info->num_colors; ++color) {
			
			int x[2], y[2];
			
			for (int i=0; i<2; ++i) {
				pos_get_coords(state->pos[color], x+i, y+i);
				cf[color].wall_dist[i] = get_wall_dist(info, x[i], y[i]);
			}

			int dx = abs(x[1]-x[0]);
			int dy = abs(y[1]-y[0]);
			
			cf[color].min_dist = dx + dy;
			
		

		}


		qsort(cf, info->num_colors, sizeof(color_features_t),
		      color_features_compare);

		for (size_t i=0; i<info->num_colors; ++i) {
			info->color_order[i] = cf[i].index;
		}
    
	}

	if (!g_options.display_quiet) {

		printf("\n************************************************"
		       "\n*               Branching Order                *\n");
		if (g_options.order_most_constrained) {
			printf("* Will choose color by most constrained\n");
		} else {
			printf("* Will choose colors in order: ");
			for (size_t i=0; i<info->num_colors; ++i) {
				int color = info->color_order[i];
				printf("%s", color_name_str(info, color));
			}
			printf("\n");
		}
		printf ("*************************************************\n\n");

	}

}

// Returns opposite direction
int opposite_dir(int dir)
{
  switch (dir) {
    case 0:
    return 1;
    case 1:
    return 0;
    case 2:
    return 3;
    case 3:
    return 2;
    }
    
    return -1;
}



// Check for free space deadends
int game_is_deadend(const game_info_t* info,
                    const game_state_t* state,
                    pos_t pos) {
  
  // Get x,y coords
  int x, y;
  pos_get_coords(pos, &x, &y);
  
  int num_free = 0;

  for (int dir=0; dir<4; ++dir) {
    pos_t neighbor_pos = offset_pos(info, x, y, dir);
    // If neighbour is not a wall and is a free space then add as a free space
    if (neighbor_pos != INVALID_POS) {
      if (!state->cells[neighbor_pos]) {
        ++num_free;
      } 
      else { // If neigbour is an UNCOMPELTED goal position or a current position of a color
             // then add as a free space
        for (size_t color=0; color<info->num_colors; ++color) {
          if (state->completed & (1 << color)) {
            continue;
          }
          if (neighbor_pos == state->pos[color] ||
              neighbor_pos == info->goal_pos[color]) {
            ++num_free;
          }
        }
                                                                 
      }
    }
  }
  
  return num_free <= 1;

}

  

//////////////////////////////////////////////////////////////////////
// Check for dead-end regions of freespace where there is no way to
// put an active path into and out of it. Any freespace node which
// has only one free neighbor represents such a dead end. For the
// purposes of this check, cur and goal positions count as "free".
// Influence taken from https://github.com/mzucker/flow_solver
int game_check_deadends(const game_info_t* info,
     
                        const game_state_t* state, int last_dir) {
  // Current position color.  
  size_t color = state->last_color;
  
  if (color >= info->num_colors) { return 0; }
  // Current position
  pos_t cur_pos = state->pos[color];

  // Get x, y coords
  int x, y;
  pos_get_coords(cur_pos, &x, &y);
  
  // Dont check directon you came from as we know its a path
  int dont_go = opposite_dir(last_dir);

  // Determine if one of the 3 spaces around current position is a dead end
  for (int dir=0; dir<4; ++dir) {
    
    if(dir == dont_go) {
      continue;
    }
    
    // Get position of neigbour
    pos_t neighbor_pos = offset_pos(info, x, y, dir);
    
    // If not a wall AND is a free space AND is a deadend.
    if (neighbor_pos != INVALID_POS &&
        !state->cells[neighbor_pos] &&
        game_is_deadend(info, state, neighbor_pos)) {
      return 1;
    }
    
    // Another classification of a deadend:
    // A goal or inital space, surronded by no free spaces AND not completed
    // is a deadend state
    if(neighbor_pos != INVALID_POS && 
    (state->cells[neighbor_pos] == 2 || state->cells[neighbor_pos] == 3) && 
    game_num_free_pos(info, state, neighbor_pos) == 0 && 
    !(state->completed & (1 << cell_get_color(state->cells[neighbor_pos]))) ) {
      return 1;
    }
                                      
  }
  // Reaching here means no dead ends found
  return 0;

}