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

// ANSI color codes for weather radar
#define COLOR_RESET   "\033[0m"
#define COLOR_BLUE    "\033[38;5;27m"      // Light rain (0.5-2 mm/h)
#define COLOR_CYAN    "\033[38;5;51m"      // Moderate rain (2-5 mm/h)
#define COLOR_GREEN   "\033[38;5;46m"      // Heavy rain (5-10 mm/h)
#define COLOR_YELLOW  "\033[38;5;226m"     // Very heavy rain (10-20 mm/h)
#define COLOR_ORANGE  "\033[38;5;208m"     // Intense rain (20-40 mm/h)
#define COLOR_RED     "\033[38;5;196m"     // Extreme rain (>40 mm/h)
#define COLOR_MAGENTA "\033[38;5;201m"     // Hail

// Weather intensity levels
typedef enum {
	WEATHER_NONE = 0,
	WEATHER_LIGHT = 1,
	WEATHER_MODERATE = 2,
	WEATHER_HEAVY = 3,
	WEATHER_VERY_HEAVY = 4,
	WEATHER_INTENSE = 5,
	WEATHER_EXTREME = 6
} WeatherIntensity;

// Matrix structure with compensation for character aspect ratio and weather data
typedef struct {
	int height;     // Number of rows (vertical)
	int width;      // Number of columns (horizontal)
	char **data;
	WeatherIntensity **weather;  // Weather intensity at each position
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
	matrix->weather = malloc(matrix->height * sizeof(WeatherIntensity*));
	for (int i = 0; i < matrix->height; i++) {
		matrix->data[i] = malloc(matrix->width * sizeof(char));
		matrix->weather[i] = malloc(matrix->width * sizeof(WeatherIntensity));
		for (int j = 0; j < matrix->width; j++) {
			matrix->data[i][j] = ' ';
			matrix->weather[i][j] = WEATHER_NONE;
		}
	}

	return matrix;
}

void free_matrix(Matrix *matrix) {
	for (int i = 0; i < matrix->height; i++) {
		free(matrix->data[i]);
		free(matrix->weather[i]);
	}
	free(matrix->data);
	free(matrix->weather);
	free(matrix);
}

// Get color code for weather intensity
const char* get_weather_color(WeatherIntensity intensity) {
	switch(intensity) {
		case WEATHER_LIGHT: return COLOR_BLUE;
		case WEATHER_MODERATE: return COLOR_CYAN;
		case WEATHER_HEAVY: return COLOR_GREEN;
		case WEATHER_VERY_HEAVY: return COLOR_YELLOW;
		case WEATHER_INTENSE: return COLOR_ORANGE;
		case WEATHER_EXTREME: return COLOR_RED;
		default: return "";
	}
}

// Get weather character based on intensity (returns UTF-8 string)
const char* get_weather_char(WeatherIntensity intensity) {
	switch(intensity) {
		case WEATHER_LIGHT: return ".";
		case WEATHER_MODERATE: return ":";
		case WEATHER_HEAVY: return "░";
		case WEATHER_VERY_HEAVY: return "▒";
		case WEATHER_INTENSE: return "▓";
		case WEATHER_EXTREME: return "█";
		default: return " ";
	}
}

// Fetch weather radar data from MeteoSwiss/Existenz API (simplified)
// This uses the free existenz.ch API which aggregates MeteoSwiss data
int fetch_weather_data(Matrix *matrix) {
	// For demonstration, we'll create a simple pattern
	// In production, you would fetch from: https://api.existenz.ch/apiv1/smn/...
	// Or parse MeteoSwiss STAC API radar data
	
	// Create a simple simulated weather pattern around Zurich
	int center_x = matrix->width / 4;  // Center of display
	int center_y = matrix->height / 2;
	
	// Add some random weather cells
	srand(time(NULL));
	int num_cells = 3 + (rand() % 5);  // 3-7 weather cells
	
	for (int cell = 0; cell < num_cells; cell++) {
		// Random position within range
		int cell_x = center_x - 30 + (rand() % 60);
		int cell_y = center_y - 30 + (rand() % 60);
		int radius = 5 + (rand() % 15);
		WeatherIntensity intensity = 1 + (rand() % 5);  // Random intensity
		
		// Draw weather cell
		for (int y = 0; y < matrix->height; y++) {
			for (int x = 0; x < matrix->width; x++) {
				int dx = x - cell_x * 2;  // Account for 2x width
				int dy = y - cell_y;
				double distance = sqrt(dx * dx / 4.0 + dy * dy);  // Elliptical
				
				if (distance < radius) {
					// Intensity decreases with distance from center
					double fade = 1.0 - (distance / radius);
					WeatherIntensity cell_intensity = (WeatherIntensity)((int)(intensity * fade));
					
					if (cell_intensity > matrix->weather[y][x]) {
						matrix->weather[y][x] = cell_intensity;
					}
				}
			}
		}
	}
	
	return 0;
}

// Alternative: Fetch real weather data from MeteoSwiss Open Data (commented out - requires HDF5 library)
/*
int fetch_meteoswiss_radar_data(Matrix *matrix) {
	// MeteoSwiss provides radar data through STAC API
	// Collection: ch.meteoschweiz.ccs4r2nj (Radar-based precipitation)
	// Format: HDF5 files updated every 5 minutes
	// API: https://data.geo.admin.ch/api/stac/v1/collections/ch.meteoschweiz.ogd-radar-rzc/items
	
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

	// Get latest radar data item
	char url[] = "https://data.geo.admin.ch/api/stac/v1/collections/ch.meteoschweiz.ogd-radar-rzc/items?limit=1";

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "aircraft-radar-display/1.0");
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

	// Parse JSON to get HDF5 file URL
	// Download and parse HDF5 file (requires libhdf5)
	// Map radar data to matrix coordinates
	
	free(chunk.memory);
	return 0;
}
*/

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
			matrix->weather[i][j] = WEATHER_NONE;
		}
	}
}

// Sonar sweep update - progressively reveals the source matrix with a sweeping effect
void sonar_sweep_update(Matrix *dest, Matrix *source) {
	int center_x = dest->width / 2;
	int center_y = dest->height / 2;
	
	int max_radius = (int)sqrt(center_x * center_x + center_y * center_y) + 1;
	int num_angles = 720;
	int usleep_per_angle = 7000;
	
	for (int angle_step = 0; angle_step < num_angles; angle_step++) {
		double theta = (angle_step * 2 * M_PI) / num_angles;
		
		for (int radius = 0; radius <= max_radius; radius++) {
			int x = center_x + (int)(radius * cos(theta));
			int y = center_y + (int)(radius * sin(theta));
			
			if (x >= 0 && x < dest->width && y >= 0 && y < dest->height) {
				dest->data[y][x] = source->data[y][x];
				dest->weather[y][x] = source->weather[y][x];
			}
		}
		
		int ret = system("clear");
		(void)ret;
		
		// Print with weather overlay
		for (int i = 0; i < dest->height; i++) {
			for (int j = 0; j < dest->width; j++) {
				// If there's aircraft data, display it (white)
				if (dest->data[i][j] != ' ') {
					printf("%s%c%s", COLOR_RESET, dest->data[i][j], COLOR_RESET);
				}
				// Otherwise show weather if present
				else if (dest->weather[i][j] != WEATHER_NONE) {
					const char *color = get_weather_color(dest->weather[i][j]);
					const char *weather_char = get_weather_char(dest->weather[i][j]);
					printf("%s%s%s", color, weather_char, COLOR_RESET);
				}
				// Otherwise show blank space
				else {
					printf(" ");
				}
			}
			printf("\n");
		}
		fflush(stdout);
		
		usleep(usleep_per_angle);
	}
}

// Print matrix with weather overlay
void print_matrix(Matrix *matrix) {
	int ret = system("clear");
	(void)ret;

	for (int i = 0; i < matrix->height; i++) {
		for (int j = 0; j < matrix->width; j++) {
			// If there's aircraft data, display it (white)
			if (matrix->data[i][j] != ' ') {
				printf("%s%c%s", COLOR_RESET, matrix->data[i][j], COLOR_RESET);
			}
			// Otherwise show weather if present
			else if (matrix->weather[i][j] != WEATHER_NONE) {
				const char *color = get_weather_color(matrix->weather[i][j]);
				const char *weather_char = get_weather_char(matrix->weather[i][j]);
				printf("%s%s%s", color, weather_char, COLOR_RESET);
			}
			// Otherwise show blank space
			else {
				printf(" ");
			}
		}
		printf("\n");
	}
}

// Convert lat/lon to screen coordinates
void latlon_to_screen(double lat, double lon, int *screen_x, int *screen_y, int width, int height) {
	double lat_diff = lat - LSZH_LAT;
	double lon_diff = lon - LSZH_LON;

	double y_nm = lat_diff * 60.0;
	double x_nm = lon_diff * 60.0 * cos(LSZH_LAT * M_PI / 180.0);

	*screen_x = (width / 4) + (int)(x_nm * (width / 4) / RANGE_NM);
	*screen_y = (height / 2) - (int)(y_nm * (height / 2) / RANGE_NM);

	if (*screen_x < 0) *screen_x = 0;
	if (*screen_x >= width / 2) *screen_x = (width / 2) - 1;
	if (*screen_y < 6) *screen_y = 6;
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

	double lat_range = RANGE_NM / 60.0;
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

		json_t *callsign_json = json_array_get(state, 1);
		json_t *lon_json = json_array_get(state, 5);
		json_t *lat_json = json_array_get(state, 6);
		json_t *altitude_json = json_array_get(state, 7);
		json_t *velocity_json = json_array_get(state, 9);

		if(!json_is_string(callsign_json) || !json_is_number(lat_json) || 
		   !json_is_number(lon_json) || !json_is_number(altitude_json)) {
			continue;
		}

		Aircraft *ac = &(*aircraft_list)[*count];

		const char *cs = json_string_value(callsign_json);
		strncpy(ac->callsign, cs, sizeof(ac->callsign) - 1);
		ac->callsign[sizeof(ac->callsign) - 1] = '\0';
		
		int len = strlen(ac->callsign);
		while(len > 0 && ac->callsign[len-1] == ' ') {
			ac->callsign[--len] = '\0';
		}

		ac->latitude = json_number_value(lat_json);
		ac->longitude = json_number_value(lon_json);
		ac->altitude = json_number_value(altitude_json);
		ac->velocity = json_is_number(velocity_json) ? json_number_value(velocity_json) : 0.0;
		ac->squawk = 0;

		ac->distance = calculate_distance(LSZH_LAT, LSZH_LON, ac->latitude, ac->longitude);

		if(ac->distance <= RANGE_NM) {
			(*count)++;
		}
	}

	json_decref(root);
	return 0;
}

int main() {
	printf("ADS-B Aircraft Display with MeteoSwiss Weather Radar - LSZH (Zurich Airport)\n");
	printf("Range: %.0f nautical miles\n", RANGE_NM);
	printf("Weather data: Simulated radar (Source: MeteoSwiss)\n");
	printf("================================================================================\n\n");
	printf("Connecting to OpenSky Network API...\n\n");

	Matrix *screen = create_square_matrix(120);
	Matrix *temp_screen = create_square_matrix(120);
	
	clear_matrix(screen);
	clear_matrix(temp_screen);
	
	int center_x = screen->width / 2;
	int center_y = screen->height / 2;
	int max_radius = (int)sqrt(center_x * center_x + center_y * center_y) + 1;
	int num_angles = 720;
	int current_angle = 0;
	
	time_t last_fetch_time = 0;
	time_t last_weather_fetch = 0;
	int fetch_interval = 10;
	int weather_fetch_interval = 60;  // Update weather every 60 seconds

	while(1) {
		time_t current_time = time(NULL);
		
		// Fetch weather data periodically
		if (current_time - last_weather_fetch >= weather_fetch_interval) {
			fetch_weather_data(temp_screen);
			last_weather_fetch = current_time;
		}
		
		// Check if it's time to fetch new aircraft data
		if (current_time - last_fetch_time >= fetch_interval) {
			Aircraft *aircraft_list = NULL;
			int aircraft_count = 0;

			if(fetch_aircraft_data(&aircraft_list, &aircraft_count) == 0) {
				// Clear only the aircraft data, keep weather
				for (int i = 0; i < temp_screen->height; i++) {
					for (int j = 0; j < temp_screen->width; j++) {
						temp_screen->data[i][j] = ' ';
					}
				}

				// Display title at top
				char title[100];
				snprintf(title, sizeof(title), "LSZH - Aircraft: %d | Weather: MeteoSwiss Radar (Simulated)", aircraft_count);
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

					int altitude_ft = (int)(ac->altitude * 3.28084);
					int speed_kts = (int)(ac->velocity * 1.94384);

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
		
		for (int radius = 0; radius <= max_radius; radius++) {
			int x = center_x + (int)(radius * cos(theta));
			int y = center_y + (int)(radius * sin(theta));
			
			if (x >= 0 && x < screen->width && y >= 0 && y < screen->height) {
				screen->data[y][x] = temp_screen->data[y][x];
				screen->weather[y][x] = temp_screen->weather[y][x];
			}
		}
		
		// Print updated display with colors
		int ret = system("clear");
		(void)ret;
		for (int i = 0; i < screen->height; i++) {
			for (int j = 0; j < screen->width; j++) {
				// Aircraft data takes priority (shown in white)
				if (screen->data[i][j] != ' ') {
					printf("%s%c%s", COLOR_RESET, screen->data[i][j], COLOR_RESET);
				}
				// Weather radar shown underneath
				else if (screen->weather[i][j] != WEATHER_NONE) {
					const char *color = get_weather_color(screen->weather[i][j]);
					const char *weather_char = get_weather_char(screen->weather[i][j]);
					printf("%s%s%s", color, weather_char, COLOR_RESET);
				}
				else {
					printf(" ");
				}
			}
			printf("\n");
		}
		fflush(stdout);
		
		current_angle = (current_angle + 1) % num_angles;
		usleep(7000);
	}

	free_matrix(screen);
	free_matrix(temp_screen);
	return 0;
}