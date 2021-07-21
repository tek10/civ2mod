#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#define FALSE 0
#define TRUE 1
#define byte unsigned char
#define DEBUG FALSE

#define PLAYERS_CIV_OFFSET  41
#define DIFFICULTY_LEVEL_OFFSET 44
#define BARB_LEVEL_OFFSET  45
#define CIVS_ACTIVE_OFFSET 46
#define TOTAL_UNITS_OFFSET 58
#define TOTAL_CITIES_OFFSET 60

// city constants
#define CITY_ITEM_NAME_OFFSET 32
#define CITY_ITEM_OWNER_OFFSET 8
#define CITY_ITEM_SIZE 88

// map constants
#define MAP_HEADER_OFFSET 13702
#define MAP_DATA_OFFSET 13716
#define MAP_BLOCK2_ITEM_SIZE 6
#define MAP_ITEM_COUNTER 3
#define MAP_ITEM_VISIBILITY 4
#define MAP_ITEM_OWNER 5
#define MAP_ITEM_TERRAIN 0
#define MAP_TYPE_TERRAIN_OCEAN 10

// unit constants
#define UNIT_OWNER_OFFSET 7
#define UNIT_TYPE_OFFSET 6
#define UNIT_HOMECITY_OFFSET 16
#define UNIT_ITEM_SIZE 32


/*
 * Program to modify save game files from Civilization II (MGE)
 *
 * Written by TE Kimball
 *
 */

byte *data;
size_t datasize;
char *inputfile;
char *outputfile;

unsigned short mapSize;
unsigned short mapWidth2;
unsigned short mapHeight;
unsigned short mapWidth;
unsigned map_block2_offset;
unsigned mapBlock2Size;
unsigned map_block3_offset;
unsigned unit_section_offset;
unsigned short totalUnits;
unsigned city_section_offset;
unsigned short totalCities;
unsigned after_city_section_offset;

void menu();
void setLocation(unsigned offset, byte value);
byte getLocation(unsigned offset);
unsigned short getLocationAsShort(unsigned offset);
unsigned short getShort(byte *ptr);
unsigned getOffsetFromPtr(byte *ptr);
byte askDecimalValue(char *prompt);
byte askHexValue(char *prompt);
char* askStringValue(char *prompt);
char* askForCity(char *prompt);
unsigned hasbit(byte bite, unsigned bitnum);
byte setbit(byte bite, unsigned bitnum);
byte setbitoff(byte bite, unsigned bitnum);
void writefile(char *filename);
byte* readfile(char *filename);
char *memmem(const void *haystack, size_t haystacklen,
                    const void *needle, size_t needlelen);
char *civColor(byte civNumber);
void copyMap(byte tociv, byte fromciv);
void wipeMap(byte civnum);
void visibleMap(byte civnum);
void setCityOwner(char *cityname, byte owner, int option);
void setContinentOwner(char *cityname, byte newOwner);
byte getMapItem(unsigned short xcoord, unsigned short ycoord, byte item_offset);
void setMapItem(unsigned short xcoord, unsigned short ycoord, byte item_offset, byte newValue);
void setOceanUnits(unsigned cityId, byte civnum);
unsigned getCityId(unsigned dataOffset);
byte *findCityItem(char *cityname);
void addCityToVisibilityMap(byte *cityblockptr, byte civnum);
void addToVisibilityMap(short unsigned xcoord, unsigned short ycoord, byte civnum, unsigned radius);
void setMapItemVisible(short xcoord, short ycoord, byte civnum);


int main(int argc, char *argv[]) {

	if (argc < 2) {
		printf("usage: %s savefile [newfile]\n", argv[0]);
		exit(0);
	}

	// read in data
	inputfile = argv[1];
	data = readfile(inputfile);

	// get some constant global values
	outputfile = inputfile;
	if (argc > 2) 
		outputfile = argv[2];
	mapSize = getLocationAsShort(MAP_HEADER_OFFSET+4);
	mapWidth2 = getLocationAsShort(MAP_HEADER_OFFSET);
	mapHeight = getLocationAsShort(MAP_HEADER_OFFSET+2);
	mapWidth = mapWidth2/2;
	map_block2_offset = MAP_DATA_OFFSET + (mapSize*7);
#ifdef DEBUG
	printf("map block2 section offset = %d\n", map_block2_offset);
#endif
	mapBlock2Size = mapSize*MAP_BLOCK2_ITEM_SIZE;
	map_block3_offset = map_block2_offset + mapBlock2Size;
#ifdef DEBUG
	printf("map block3 section offset = %d\n", map_block3_offset);
#endif
	unsigned short mapheader6 = getLocationAsShort(MAP_HEADER_OFFSET+10);
	unsigned short mapheader7 = getLocationAsShort(MAP_HEADER_OFFSET+12);
	unsigned mapBlock3Size = mapheader6*mapheader7*2;
	unit_section_offset = map_block3_offset + mapBlock3Size + 1024;
#ifdef DEBUG
	printf("unit section offset = %d\n", unit_section_offset);
#endif

	totalUnits = getLocationAsShort(TOTAL_UNITS_OFFSET);
#ifdef DEBUG
	printf("total units = %d\n", totalUnits);
#endif
	city_section_offset = unit_section_offset + (totalUnits*UNIT_ITEM_SIZE); 
#ifdef DEBUG
	printf("city section offset = %d\n", city_section_offset);
#endif
	totalCities= getLocationAsShort(TOTAL_CITIES_OFFSET);
#ifdef DEBUG
	printf("total cities = %d\n", totalCities);
#endif
	after_city_section_offset = city_section_offset + (totalCities*CITY_ITEM_SIZE); 
#ifdef DEBUG
	printf("after city section offset = %d\n", after_city_section_offset);
#endif

	menu();

}



void menu() {

	int choice = 0;
	char *city = NULL;
	char *quit = "n";
	byte player_civ = getLocation(PLAYERS_CIV_OFFSET);
	byte civnum;

	do {

		byte active_civ_mask = getLocation(CIVS_ACTIVE_OFFSET);

		printf("\n");
		printf("Player Civilization:  %d-%s\n", player_civ, civColor(player_civ));
		printf("\n");
		printf("0 Exit\n");
		printf("1 Save changes to %s\n", outputfile);
		printf("2 Change barbarian activity level (0-3) = %d\n", getLocation(BARB_LEVEL_OFFSET));
		printf("3 Change difficulity Level (0-5) = %d\n", getLocation(DIFFICULTY_LEVEL_OFFSET));
		printf("4 Change active civilizations (mask = 0x%02x)\n", active_civ_mask);

		printf("5 Add an active civilization\n");
		printf("   Active civilizations are: \n");
		for (int i=0;i<8; i++) {
			if (hasbit(active_civ_mask,i)) 
				printf("    %d-%s\n", i, civColor(i));
		}

		printf("6 Change city and units inside city to new owner\n");
		printf("7 Change city and units owned by city to new owner\n");
		printf("8 Change ownership of all cities and units on a continent\n");
		printf("9 Extend a visibilty map with the map from another civilization\n");
		printf("10 Make entire map invisible\n");
		printf("11 Make entire map visible\n");
		

		byte choice = askDecimalValue("Enter>");
		byte value = 0;
		byte mask;
		byte flag;

		switch (choice) {
			case 0: 
				quit = askStringValue("Really Exit? (y/n)");
				if (quit[0] == 'y')
					exit(0);
				break;
			case 1:
				writefile(outputfile);
				printf("*** Changes saved to %s ***\n", outputfile);
				break;
			case 2:
				printf("\n 0=villages only  1=roving bands 2=restless bands 3=raging hordes\n");
				value = askDecimalValue("New Barbarian Level: (0-3)");
				if (value < 4)
					setLocation(BARB_LEVEL_OFFSET, value);
				break;
			case 3:
				printf("\n 0=Chieftain 1-Warlord 2=Prince 3=King 4=Emperor 5=Deity\n");
				value = askDecimalValue("New Difficulty Level: (0-5)");
				if (value < 6)
					setLocation(DIFFICULTY_LEVEL_OFFSET, value);
				break;
			case 4:
				printf("\n MASK VALUES: 0x01=barbarian 0x02=white 0x04=green 0x08=blue 0x0x10=yellow 0x20=cyan 0x40=orange 0x80=purple\n");
				value = askHexValue("Enter new (hex) mask: ");
				setLocation(CIVS_ACTIVE_OFFSET, value);
				break;
			case 5:
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");


				value = askDecimalValue("Enter Civ Number: ");
				active_civ_mask = setbit(active_civ_mask,value);
				setLocation(CIVS_ACTIVE_OFFSET, active_civ_mask);
				break;
			case 6:
				city = askForCity("Enter City Name:");
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");

				value = askDecimalValue("Enter New City Owner Number: ");
				if (value < 8)
					setCityOwner(city, value, 0);
				free(city);
				break;
			case 7:
				city = askForCity("Enter City Name:");
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");

				value = askDecimalValue("Enter New City Owner Number: ");
				if (value < 8)
					setCityOwner(city, value, 1);
				free(city);
				break;
			case 8:
				city = askForCity("Enter a City Name in the Continent:");
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");

				value = askDecimalValue("Enter Civ Number: ");
				if (value < 8)
					setContinentOwner(city, value);
				free(city);
				break;
			case 9:
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");

				byte fromciv = askDecimalValue("Enter Civ Number to copy map from: ");

				byte tociv = askDecimalValue("Enter Civ Number to copy map to: ");
				copyMap(tociv, fromciv);

				break;
			case 10:
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");

				civnum = askDecimalValue("Enter Civ Number: ");
				wipeMap(civnum);
				break;
			case 11:
				printf("\n");
				for (int i=0; i<8; i++) {
					printf("%d-%s ",i,civColor(i));
				}
				printf("\n");

				civnum = askDecimalValue("Enter Civ Number: ");
				visibleMap(civnum);
				break;

		}

	} while (TRUE);

	exit(0);

}

void wipeMap(byte civnum) {

	byte visibility = 0;

	for (unsigned offset = map_block2_offset; offset< map_block3_offset; offset=offset+MAP_BLOCK2_ITEM_SIZE) {
		visibility = getLocation(offset + MAP_ITEM_VISIBILITY);
		visibility = setbitoff(visibility, civnum);
		setLocation(offset + MAP_ITEM_VISIBILITY, visibility);
	}
}

void visibleMap(byte civnum) {

	byte visibility = 0;

	for (unsigned offset = map_block2_offset; offset< map_block3_offset; offset=offset+MAP_BLOCK2_ITEM_SIZE) {
		visibility = getLocation(offset + MAP_ITEM_VISIBILITY);
		visibility = setbit(visibility, civnum);
		setLocation(offset + MAP_ITEM_VISIBILITY, visibility);
	}
}

void copyMap(byte tociv, byte fromciv) {

	byte visibility = 0;

	for (unsigned offset = map_block2_offset; offset< map_block3_offset; offset=offset+MAP_BLOCK2_ITEM_SIZE) {
		visibility = getLocation(offset + MAP_ITEM_VISIBILITY);
		if (hasbit(visibility, fromciv))
			visibility = setbit(visibility, tociv);
		setLocation(offset + MAP_ITEM_VISIBILITY, visibility);
	}
}


byte getMapItem(unsigned short xcoord, unsigned short ycoord, byte item_offset) {

	unsigned short map_item_offset = ((ycoord*mapWidth) + (xcoord)/2)*MAP_BLOCK2_ITEM_SIZE;
	unsigned short total_offset = map_block2_offset + map_item_offset + item_offset;
	byte item = getLocation(total_offset);

	return item;
}

void setMapItem(unsigned short xcoord, unsigned short ycoord, byte item_offset, byte newValue) {

	unsigned short map_item_offset = ((ycoord*mapWidth) + (xcoord)/2)*MAP_BLOCK2_ITEM_SIZE;
	unsigned short total_offset = map_block2_offset + map_item_offset + item_offset;
	setLocation(total_offset, newValue);
}



// assign all cities/units on continent containing city to new owner
void setContinentOwner(char *cityname, byte newOwner) {


	byte *cityblockptr = findCityItem(cityname);
	if (cityblockptr == NULL) 
		return;

	short xcoord = (short) getShort(cityblockptr);
	short ycoord = (short) getShort(cityblockptr+2);
	
	byte cityBodyCounter = getMapItem(xcoord, ycoord, MAP_ITEM_COUNTER);

	byte mapOwner;
	byte visibility;

	// set map visibilty and ownership in all squares in continent 
	for (unsigned offset = map_block2_offset; offset< map_block3_offset; offset=offset+MAP_BLOCK2_ITEM_SIZE) {
		byte counter = getLocation(offset + MAP_ITEM_COUNTER);
		if (counter == cityBodyCounter) {

			byte mapType = getLocation(offset + MAP_ITEM_TERRAIN);
			if (mapType == MAP_TYPE_TERRAIN_OCEAN) {
				continue;
			}

			//visibility
			visibility = getLocation(offset + MAP_ITEM_VISIBILITY);
			visibility = setbit(visibility, newOwner);
			setLocation(offset + MAP_ITEM_VISIBILITY, visibility);

			//ownership; set first 4 bits
			mapOwner = getLocation(offset + MAP_ITEM_OWNER);
			mapOwner = setbitoff(mapOwner, 4);
			mapOwner = setbitoff(mapOwner, 5);
			mapOwner = setbitoff(mapOwner, 6);
			mapOwner = setbitoff(mapOwner, 7);
			mapOwner = mapOwner + (newOwner*16);
			setLocation(offset + MAP_ITEM_OWNER, mapOwner);
		}
	}


	// set all units in continent to new owner
	for (unsigned offset = unit_section_offset; offset < city_section_offset; offset=offset+UNIT_ITEM_SIZE) {
		xcoord = getLocationAsShort(offset);
		ycoord = getLocationAsShort(offset+2);
		byte bodyCounter = getMapItem(xcoord, ycoord, MAP_ITEM_COUNTER);
		if (bodyCounter == cityBodyCounter) {
			data[offset+UNIT_OWNER_OFFSET] = newOwner;
		}
	}

	// set all cities in continent to new owner
	for (unsigned offset = city_section_offset; offset < after_city_section_offset; offset=offset+CITY_ITEM_SIZE) {
		xcoord = getLocationAsShort(offset);
		ycoord = getLocationAsShort(offset+2);
		byte bodyCounter = getMapItem(xcoord, ycoord, MAP_ITEM_COUNTER);
		if (bodyCounter == cityBodyCounter) {
			data[offset+CITY_ITEM_OWNER_OFFSET] = newOwner;
			setOceanUnits(getCityId(offset), newOwner);
			addCityToVisibilityMap(data+offset, newOwner);
		}
	}
}


//get city id number from data offset of city item
unsigned getCityId(unsigned dataOffset) {
	assert(dataOffset >= city_section_offset);
	unsigned cityItemOffset = dataOffset - city_section_offset;
	unsigned cityId = cityItemOffset/CITY_ITEM_SIZE;
	return cityId;
}

// set all ships/aircraft in open water belonging to city to new cityowner
void setOceanUnits(unsigned cityId, byte civnum) {

	unsigned short id = 0xFF;
	unsigned short xcoord;
	unsigned short ycoord;

	// set all city units in ocean to new owner
	for (unsigned offset = unit_section_offset; offset < city_section_offset; offset=offset+UNIT_ITEM_SIZE) {

		id = getLocationAsShort(offset+UNIT_HOMECITY_OFFSET);
		if (id != cityId)
			continue;

		xcoord = getLocationAsShort(offset);
		ycoord = getLocationAsShort(offset+2);
		byte mapType = getMapItem(xcoord, ycoord, MAP_ITEM_TERRAIN);

		if (mapType == MAP_TYPE_TERRAIN_OCEAN) {
			setLocation(offset+UNIT_OWNER_OFFSET, civnum);
			// make unit visible on map with radius 1
			addToVisibilityMap(xcoord, ycoord, civnum, 1);
#ifdef DEBUG
			printf("ocean unit type %d found\n", data[offset+UNIT_TYPE_OFFSET]);
#endif
		}
	}
}

// set all units belonging to city to new owner
void setUnitsFromCity(unsigned cityId, byte civnum) {

	unsigned short id = 0xFF;
	
	for (unsigned offset = unit_section_offset; offset < city_section_offset; offset=offset+UNIT_ITEM_SIZE) {

		id = getLocationAsShort(offset+UNIT_HOMECITY_OFFSET);
		if (id != cityId)
			continue;

		setLocation(offset+UNIT_OWNER_OFFSET, civnum);
#ifdef DEBUG
		printf("city owned unit type %d found\n", data[offset+UNIT_TYPE_OFFSET]);
#endif

		// make unit visible on map with radius 1
		unsigned short xcoord = (short) getLocationAsShort(offset);
		unsigned short ycoord = (short) getLocationAsShort(offset+2);
		addToVisibilityMap(xcoord, ycoord, civnum, 1);
	}
}

// set all units inside city to new owner
void setUnitsInCity(unsigned cityId, byte civnum) {

	unsigned cityItemOffset = city_section_offset + (cityId*CITY_ITEM_SIZE);
	
	for (unsigned offset = unit_section_offset; offset < city_section_offset; offset=offset+UNIT_ITEM_SIZE) {

		int cmp = memcmp(data+cityItemOffset,data+offset,4);
		if (cmp != 0)
			continue;

		setLocation(offset+UNIT_OWNER_OFFSET, civnum);
#ifdef DEBUG
		printf("unit type %d found in city\n", data[offset+UNIT_TYPE_OFFSET]);
#endif
	}
}


// find city item ptr from city name
byte *findCityItem(char *cityname) {
	char *nameptr = memmem(data+city_section_offset, after_city_section_offset - city_section_offset, cityname, strlen(cityname));

	if (nameptr == NULL) {
		printf("*** City %s not found ***\n", cityname);
		return NULL;
	}
	byte *cityblockptr = nameptr - CITY_ITEM_NAME_OFFSET;
	return cityblockptr;
}


void setCityOwner(char *cityname, byte newOwner, int option) {

	byte *ownerptr;
	byte *cityblockptr;  

	cityblockptr = findCityItem(cityname);
	if (cityblockptr == NULL) 
		return;

	ownerptr = cityblockptr + CITY_ITEM_OWNER_OFFSET;
	*ownerptr = newOwner;

	addCityToVisibilityMap(cityblockptr, newOwner);

	unsigned cityId = getCityId(getOffsetFromPtr(cityblockptr));

	if (option == 0) {
		setUnitsInCity(cityId, newOwner);
		setOceanUnits(cityId, newOwner);
	}
	if (option == 1) {
		setUnitsFromCity(cityId, newOwner);
	}

}

void addCityToVisibilityMap(byte *cityblockptr, byte civnum) {
	short xcoord = getShort(cityblockptr);
	short ycoord = getShort(cityblockptr+2);
	addToVisibilityMap(xcoord, ycoord, civnum, 3);
}

void addToVisibilityMap(unsigned short xcoordp, unsigned short ycoordp, byte civnum, unsigned radius) {

	short xcoord = (short) xcoordp;
	short ycoord = (short) ycoordp;

	//radious == 0
	setMapItemVisible(xcoord, ycoord, civnum);

	if (radius < 1)
		return;

	//radious == 1
	setMapItemVisible(xcoord-1, ycoord-1, civnum);
	setMapItemVisible(xcoord+1, ycoord-1, civnum);
	setMapItemVisible(xcoord+1, ycoord+1, civnum);
	setMapItemVisible(xcoord-1, ycoord+1, civnum);
	setMapItemVisible(xcoord, ycoord-2, civnum);
	setMapItemVisible(xcoord, ycoord+2, civnum);
	setMapItemVisible(xcoord+2, ycoord, civnum);
	setMapItemVisible(xcoord-2, ycoord, civnum);

	if (radius < 2)
		return;

	//radious == 2
	setMapItemVisible(xcoord-1, ycoord-3, civnum);
	setMapItemVisible(xcoord+1, ycoord-3, civnum);
	setMapItemVisible(xcoord-2, ycoord-2, civnum);
	setMapItemVisible(xcoord+2, ycoord-2, civnum);
	setMapItemVisible(xcoord-3, ycoord-1, civnum);
	setMapItemVisible(xcoord+3, ycoord-1, civnum);
	setMapItemVisible(xcoord-3, ycoord+1, civnum);
	setMapItemVisible(xcoord+3, ycoord+1, civnum);
	setMapItemVisible(xcoord-2, ycoord+2, civnum);
	setMapItemVisible(xcoord+2, ycoord+2, civnum);
	setMapItemVisible(xcoord-1, ycoord+3, civnum);
	setMapItemVisible(xcoord+1, ycoord+3, civnum);
}

void setMapItemVisible(short xcoord, short ycoord, byte civnum) {

#ifdef DEBUG
	printf("xcoord=%d ycoord=%d\n", xcoord, ycoord);
#endif

	if (xcoord < 0) {
		xcoord = xcoord + mapWidth2;
	}
	if (xcoord >= mapWidth2) {
		xcoord = xcoord - mapWidth2;
	}
	if (ycoord < 0) {
		return;
	}
	if (ycoord >= mapHeight) {
		return;
	}

#ifdef DEBUG
	printf("set to civnum %d\n", civnum);
#endif

	byte visibility = getMapItem(xcoord, ycoord, MAP_ITEM_VISIBILITY);
	visibility = setbit(visibility, civnum);
	setMapItem(xcoord, ycoord, MAP_ITEM_VISIBILITY, visibility);
}


unsigned getOffsetFromPtr(byte *ptr) {
	return ptr - data;
}

// ask user for decimal value
byte askDecimalValue(char *prompt) {
	byte value;
	int rc = 0;

	char buf[20];
	char *bufptr = NULL;

	while (rc == 0) {
		bufptr = NULL;
		printf("\n%s ", prompt);
		bufptr = fgets(buf, 20, stdin);
		fflush(stdin);
		if (bufptr == NULL)
			continue;
		rc = sscanf(buf, "%d", &value);
	}

	return value;
}

// ask user for city name 
char* askForCity(char *prompt) {
	char *value = askStringValue(prompt);
	while ((value == NULL) || (strlen(value) < 1))
		value = askStringValue(prompt);	

	// capitalize city name
	int len = strlen(value);
	value[0] = toupper(value[0]);
	for (int i=1; i<len; i++)
		value[i] = tolower(value[i]);

	return value;
}

// ask user for string value
char* askStringValue(char *prompt) {
	char *value;
	char buf[30];
	char *bufptr = NULL;


	while (bufptr == NULL) {
		printf("\n%s ", prompt);
		bufptr = fgets(buf, 30, stdin);
		fflush(stdin);
	}

	char *ptr = malloc(strlen(buf));
	strncpy(ptr,buf, strlen(buf)-1);
	return ptr;
}

// ask user for hex value
byte askHexValue(char *prompt) {
	byte value;
	int rc = 0;

	char buf[20];
	char *bufptr = NULL;

	while (rc == 0) {
		bufptr = NULL;
		printf("\n%s ", prompt);
		bufptr = fgets(buf, 20, stdin);
		fflush(stdin);
		if (bufptr == NULL)
			continue;
		rc = sscanf(buf, "%x", &value);
	}

	return value;
}

byte setbit(byte bite, unsigned bitnum) {

	//bitnum is 0-7
	if (bitnum > 8) 
		return bite;

	byte shifted = 1 << bitnum;
	return (bite | shifted);
}

byte setbitoff(byte bite, unsigned bitnum) {
	//bitnum is 0-7
	if (bitnum > 8) 
		return bite;

	byte shifted = 1 << bitnum;
	bite = bite & ~shifted;

	return bite;

}

unsigned hasbit(byte bite, unsigned bitnum) {

	//bitnum is 0-7
	if (bitnum > 8) 
		return FALSE;
	
	byte shifted = 1 << bitnum;

	return ((bite&shifted) > 0);
}


byte getLocation(unsigned offset) {
	return data[offset];
}

void setLocation(unsigned offset, byte value) {
	data[offset] = value;
}

unsigned short getLocationAsShort(unsigned offset) {

	unsigned short *ptr = (unsigned short *)(data+offset);
	return *ptr;
}

unsigned short getShort(byte *ptr) {
	unsigned short *sptr = (unsigned short *) ptr;
	return *sptr;
}


void writefile(char *filename) {
	FILE *ofp;

	ofp = fopen(filename, "wb");
	if (ofp == NULL) {
		fprintf(stderr,"Error writing file\n");
	}
	int sizewritten = fwrite(data, 1, datasize, ofp);
	if (sizewritten != datasize) {
		fprintf(stderr,"problem writing file\n");
		fprintf(stderr,"file size = %s\n", sizewritten);
	}
	fclose(ofp);
}

byte* readfile (char *filename) {

	FILE *ifp;
	char *buffer;

	ifp = fopen(filename, "rb");
	fseek(ifp,0,SEEK_END);
	datasize = ftell(ifp);
	fseek(ifp,0,SEEK_SET);

	if (ifp == NULL) {
		fprintf(stderr,"Error reading file\n");
		exit(1);
	}


	buffer=(char *)malloc(datasize+1);

	if (!buffer)
	{
		fprintf(stderr, "Memory error!");
        	fclose(ifp);
		exit(1);
	}

	fread(buffer, datasize, 1, ifp);
	fclose(ifp);

	return buffer;


}

// return civ color from civ number
char *civColor(byte civNumber) {
	switch (civNumber) {
		case 0: 
			return "Red (Barbarians)";
		case 1: 
			return "White";
		case 2: 
			return "Green";
		case 3: 
			return "Blue";
		case 4: 
			return "Yellow";
		case 5: 
			return "Cyan";
		case 6: 
			return "Orange";
		case 7: 
			return "Purple";
	}
	return ("No Such Civ");
}

