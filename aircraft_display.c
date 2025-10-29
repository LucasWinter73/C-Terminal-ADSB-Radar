#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <curl/curl.h>
#include <jansson.h>
#include <unistd.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// LSZH (Zurich Airport) coordinates
#define LSZH_LAT 47.458056
#define LSZH_LON 8.548056
#define RANGE_NM 20.0
#define EARTH_RADIUS_NM 3440.065  // Earth radius in nautical miles

// Matrix structure with compensation for character aspect ratio
typedef struct {
	int height;     // Number of rows (vertical)
	int width;      // Number of columns (horizontal)
	char **data;
} Matrix;

// Aircraft structure
typedef struct {
	char callsign[16];
	double latitude;
	double longitude;
	double altitude;    // in meters
	double velocity;    // in m/s
	int squawk;
	double distance;    // distance from LSZH in nm
} Aircraft;

// Structure for API response
struct MemoryStruct {
	char *memory;
	size_t size;
};

// Callback function for curl
static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp) {
	size_t realsize = size * nmemb;
	struct MemoryStruct *mem = (struct MemoryStruct *)userp;

	char *ptr = realloc(mem->memory, mem->size + realsize + 1);
	if(!ptr) {
		printf("Not enough memory!\n");
		return 0;
	}

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

// Calculate distance between two coordinates (Haversine formula)
double calculate_distance(double lat1, double lon1, double lat2, double lon2) {
	double dLat = (lat2 - lat1) * M_PI / 180.0;
	double dLon = (lon2 - lon1) * M_PI / 180.0;

	lat1 = lat1 * M_PI / 180.0;
	lat2 = lat2 * M_PI / 180.0;

	double a = sin(dLat / 2) * sin(dLat / 2) +
	           sin(dLon / 2) * sin(dLon / 2) * cos(lat1) * cos(lat2);
	double c = 2 * atan2(sqrt(a), sqrt(1 - a));

	return EARTH_RADIUS_NM * c;
}

// Create a visually square matrix
Matrix* create_square_matrix(int n) {
	Matrix *matrix = malloc(sizeof(Matrix));

	// Compensate for character aspect ratio
	matrix->height = n;
	matrix->width = n * 2;

	matrix->data = malloc(matrix->height * sizeof(char*));
	for (int i = 0; i < matrix->height; i++) {
		matrix->data[i] = malloc(matrix->width * sizeof(char));
		for (int j = 0; j < matrix->width; j++) {
			matrix->data[i][j] = ' ';
		}
	}

	return matrix;
}

void free_matrix(Matrix *matrix) {
	for (int i = 0; i < matrix->height; i++) {
		free(matrix->data[i]);
	}
	free(matrix->data);
	free(matrix);
}

// Display "X" at given coordinates
void display_symbol(Matrix *matrix, int x, int y) {
	int scaled_x = x * 2;

	if (scaled_x >= 0 && scaled_x < matrix->width && y >= 0 && y < matrix->height) {
		matrix->data[y][scaled_x] = 'X';
	}
}

// Display "/" one line above and one position to the right
void display_slash(Matrix *matrix, int x, int y) {
	int scaled_x = (x + 1) * 2;
	int slash_y = y - 1;

	if (scaled_x >= 0 && scaled_x < matrix->width && slash_y >= 0 && slash_y < matrix->height) {
		matrix->data[slash_y][scaled_x] = '/';
	}
}

// Display information text aligned with the slash
void display_info(Matrix *matrix, int x, int y, const char *callsign, int altitude_ft, int speed_kts, double distance_nm) {
	int slash_x = (x + 1) * 2;
	int text_y = y - 5;

	if (text_y >= 0 && text_y < matrix->height) {
		char buffer[100];

		// Callsign line
		int tab_offset = slash_x;
		snprintf(buffer, sizeof(buffer), "%.8s", callsign);
		for (int i = 0; buffer[i] != '\0' && (tab_offset + i) < matrix->width; i++) {
			matrix->data[text_y][tab_offset + i] = buffer[i];
		}

		// Altitude line
		snprintf(buffer, sizeof(buffer), "Alt:%dft", altitude_ft);
		for (int i = 0; buffer[i] != '\0' && (tab_offset + i) < matrix->width; i++) {
			matrix->data[text_y + 1][tab_offset + i] = buffer[i];
		}

		// Speed line
		snprintf(buffer, sizeof(buffer), "Spd:%dkt", speed_kts);
		for (int i = 0; buffer[i] != '\0' && (tab_offset + i) < matrix->width; i++) {
			matrix->data[text_y + 2][tab_offset + i] = buffer[i];
		}

		// Distance line
		snprintf(buffer, sizeof(buffer), "Dst:%.1fnm", distance_nm);
		for (int i = 0; buffer[i] != '\0' && (tab_offset + i) < matrix->width; i++) {
			matrix->data[text_y + 3][tab_offset + i] = buffer[i];
		}
	}
}

void clear_matrix(Matrix *matrix) {
	for (int i = 0; i < matrix->height; i++) {
		for (int j = 0; j < matrix->width; j++) {
			matrix->data[i][j] = ' ';
		}
	}
}

// Sonar sweep update - progressively reveals the source matrix with a sweeping effect
void sonar_sweep_update(Matrix *dest, Matrix *source) {
	// The actual center in the matrix is at width/2, height/2
	int center_x = dest->width / 2;
	int center_y = dest->height / 2;
	
	// DON'T clear the destination - keep old data until sonar passes over it
	
	// Maximum radius to cover the entire screen
	int max_radius = (int)sqrt(center_x * center_x + center_y * center_y) + 1;
	
	// Number of angle steps for a full 360 degree rotation
	int num_angles = 720;  // Smooth sweep
	int usleep_per_angle = 7000; // About 10 seconds for full sweep (twice as fast: 720 * 7000 = 10.08 seconds)
	
	// Sweep through all angles
	for (int angle_step = 0; angle_step < num_angles; angle_step++) {
		double theta = (angle_step * 2 * M_PI) / num_angles;
		
		// For this angle, update all radii from center to edge
		for (int radius = 0; radius <= max_radius; radius++) {
			// Calculate the point at this radius and angle
			int x = center_x + (int)(radius * cos(theta));
			int y = center_y + (int)(radius * sin(theta));
			
			// Copy this pixel from source to destination (overwriting old data)
			if (x >= 0 && x < dest->width && y >= 0 && y < dest->height) {
				dest->data[y][x] = source->data[y][x];
			}
		}
		
		// Print updated display after each angle step
		int ret = system("clear");
		(void)ret;
		for (int i = 0; i < dest->height; i++) {
			for (int j = 0; j < dest->width; j++) {
				printf("%c", dest->data[i][j]);
			}
			printf("\n");
		}
		fflush(stdout);
		
		usleep(usleep_per_angle);
	}
}


// Print matrix without borders
void print_matrix(Matrix *matrix) {
	int ret = system("clear");
	(void)ret;  // Suppress unused result warning

	// No borders - just print the content
	for (int i = 0; i < matrix->height; i++) {
		for (int j = 0; j < matrix->width; j++) {
			printf("%c", matrix->data[i][j]);
		}
		printf("\n");
	}
}

// Convert lat/lon to screen coordinates
void latlon_to_screen(double lat, double lon, int *screen_x, int *screen_y, int width, int height) {
	// Calculate relative position from LSZH
	double lat_diff = lat - LSZH_LAT;
	double lon_diff = lon - LSZH_LON;

	// Scale to nm (approximately)
	double y_nm = lat_diff * 60.0;  // 1 degree latitude ≈ 60nm
	double x_nm = lon_diff * 60.0 * cos(LSZH_LAT * M_PI / 180.0);  // Adjust for latitude

	// Convert to screen coordinates
	// Center is at (width/2, height/2)
	// Scale: RANGE_NM corresponds to half screen
	*screen_x = (width / 4) + (int)(x_nm * (width / 4) / RANGE_NM);
	*screen_y = (height / 2) - (int)(y_nm * (height / 2) / RANGE_NM);

	// Clamp to screen bounds
	if (*screen_x < 0) *screen_x = 0;
	if (*screen_x >= width / 2) *screen_x = (width / 2) - 1;
	if (*screen_y < 6) *screen_y = 6;  // Leave room for info text
	if (*screen_y >= height) *screen_y = height - 1;
}

// Fetch aircraft data from OpenSky Network API
int fetch_aircraft_data(Aircraft **aircraft_list, int *count) {
	CURL *curl;
	CURLcode res;
	struct MemoryStruct chunk;

	chunk.memory = malloc(1);
	chunk.size = 0;

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if(!curl) {
		fprintf(stderr, "Failed to initialize CURL\n");
		return -1;
	}

	// OpenSky Network API - get all states
	// Using bounding box around LSZH (approximately 20nm radius converted to degrees)
	double lat_range = RANGE_NM / 60.0;  // degrees
	double lon_range = RANGE_NM / (60.0 * cos(LSZH_LAT * M_PI / 180.0));

	char url[512];
	snprintf(url, sizeof(url),
	         "https://opensky-network.org/api/states/all?lamin=%.4f&lomin=%.4f&lamax=%.4f&lomax=%.4f",
	         LSZH_LAT - lat_range, LSZH_LON - lon_range,
	         LSZH_LAT + lat_range, LSZH_LON + lon_range);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "aircraft-display/1.0");
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

	res = curl_easy_perform(curl);

	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		curl_easy_cleanup(curl);
		curl_global_cleanup();
		free(chunk.memory);
		return -1;
	}

	curl_easy_cleanup(curl);
	curl_global_cleanup();

	// Parse JSON response
	json_error_t error;
	json_t *root = json_loads(chunk.memory, 0, &error);
	free(chunk.memory);

	if(!root) {
		fprintf(stderr, "JSON parsing error: %s\n", error.text);
		return -1;
	}

	json_t *states = json_object_get(root, "states");
	if(!json_is_array(states)) {
		fprintf(stderr, "No states array in response\n");
		json_decref(root);
		return -1;
	}

	size_t array_size = json_array_size(states);
	*aircraft_list = malloc(array_size * sizeof(Aircraft));
	*count = 0;

	for(size_t i = 0; i < array_size; i++) {
		json_t *state = json_array_get(states, i);

		// Extract data from state vector
		json_t *callsign_json = json_array_get(state, 1);
		json_t *lon_json = json_array_get(state, 5);
		json_t *lat_json = json_array_get(state, 6);
		json_t *altitude_json = json_array_get(state, 7);
		json_t *velocity_json = json_array_get(state, 9);

		// Skip if essential data is missing
		if(!json_is_string(callsign_json) || !json_is_number(lat_json) || 
		   !json_is_number(lon_json) || !json_is_number(altitude_json)) {
			continue;
		}

		Aircraft *ac = &(*aircraft_list)[*count];

		// Copy callsign and trim whitespace
		const char *cs = json_string_value(callsign_json);
		strncpy(ac->callsign, cs, sizeof(ac->callsign) - 1);
		ac->callsign[sizeof(ac->callsign) - 1] = '\0';
		
		// Trim trailing spaces
		int len = strlen(ac->callsign);
		while(len > 0 && ac->callsign[len-1] == ' ') {
			ac->callsign[--len] = '\0';
		}

		ac->latitude = json_number_value(lat_json);
		ac->longitude = json_number_value(lon_json);
		ac->altitude = json_number_value(altitude_json);
		ac->velocity = json_is_number(velocity_json) ? json_number_value(velocity_json) : 0.0;
		ac->squawk = 0;  // OpenSky doesn't provide squawk in basic API

		// Calculate distance from LSZH
		ac->distance = calculate_distance(LSZH_LAT, LSZH_LON, ac->latitude, ac->longitude);

		// Only include if within range
		if(ac->distance <= RANGE_NM) {
			(*count)++;
		}
	}

	json_decref(root);
	return 0;
}

int main() {
	printf("ADS-B Aircraft Display - LSZH (Zurich Airport)\n");
	printf("Range: %.0f nautical miles\n", RANGE_NM);
	printf("================================================\n\n");
	printf("Connecting to OpenSky Network API...\n\n");

	Matrix *screen = create_square_matrix(120);
	Matrix *temp_screen = create_square_matrix(120);  // Temporary buffer for new data
	
	// Initialize screen with spaces
	clear_matrix(screen);
	clear_matrix(temp_screen);
	
	// Sonar state
	int center_x = screen->width / 2;
	int center_y = screen->height / 2;
	int max_radius = (int)sqrt(center_x * center_x + center_y * center_y) + 1;
	int num_angles = 720;
	int current_angle = 0;
	
	time_t last_fetch_time = 0;
	int fetch_interval = 10; // Fetch data every 10 seconds

	while(1) {
		time_t current_time = time(NULL);
		
		// Check if it's time to fetch new data
		if (current_time - last_fetch_time >= fetch_interval) {
			Aircraft *aircraft_list = NULL;
			int aircraft_count = 0;

			if(fetch_aircraft_data(&aircraft_list, &aircraft_count) == 0) {
				clear_matrix(temp_screen);

				// Display title at top
				char title[100];
				snprintf(title, sizeof(title), "LSZH - Aircraft within %.0fnm - Count: %d", RANGE_NM, aircraft_count);
				for(int i = 0; title[i] != '\0' && i < temp_screen->width; i++) {
					temp_screen->data[0][i] = title[i];
				}

				// Draw center marker for LSZH
				int center_x_marker = temp_screen->width / 4;
				int center_y_marker = temp_screen->height / 2;
				if(center_y_marker >= 0 && center_y_marker < temp_screen->height && center_x_marker * 2 < temp_screen->width) {
					temp_screen->data[center_y_marker][center_x_marker * 2] = '+';
				}

				// Display each aircraft
				for(int i = 0; i < aircraft_count; i++) {
					Aircraft *ac = &aircraft_list[i];

					int screen_x, screen_y;
					latlon_to_screen(ac->latitude, ac->longitude, &screen_x, &screen_y, 
					                temp_screen->width, temp_screen->height);

					int altitude_ft = (int)(ac->altitude * 3.28084);  // meters to feet
					int speed_kts = (int)(ac->velocity * 1.94384);    // m/s to knots

					// Filter: only display aircraft above 60 knots
					if(altitude_ft <= 1800 || speed_kts <= 60) {
						continue;
					}

					display_symbol(temp_screen, screen_x, screen_y);
					display_slash(temp_screen, screen_x, screen_y);
					display_info(temp_screen, screen_x, screen_y, ac->callsign, altitude_ft, speed_kts, ac->distance);
				}

				free(aircraft_list);
			}
			
			last_fetch_time = current_time;
		}
		
		// Perform one sonar sweep step
		double theta = (current_angle * 2 * M_PI) / num_angles;
		
		// For this angle, update all radii from center to edge
		for (int radius = 0; radius <= max_radius; radius++) {
			// Calculate the point at this radius and angle
			int x = center_x + (int)(radius * cos(theta));
			int y = center_y + (int)(radius * sin(theta));
			
			// Copy this pixel from source to destination (overwriting old data)
			if (x >= 0 && x < screen->width && y >= 0 && y < screen->height) {
				screen->data[y][x] = temp_screen->data[y][x];
			}
		}
		
		// Print updated display
		int ret = system("clear");
		(void)ret;
		for (int i = 0; i < screen->height; i++) {
			for (int j = 0; j < screen->width; j++) {
				printf("%c", screen->data[i][j]);
			}
			printf("\n");
		}
		fflush(stdout);
		
		// Advance to next angle
		current_angle = (current_angle + 1) % num_angles;
		
		// Sleep between angle steps (14ms for ~10 second full rotation)
		usleep(7000);
	}

	free_matrix(screen);
	free_matrix(temp_screen);
	return 0;
}