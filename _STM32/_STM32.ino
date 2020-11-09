//
// Multicore 2
//
// Copyright (c) 2017-2020 - Victor Trucco
//
// Additional code, debug and fixes: Diogo Patrão
//
// All rights reserved
//
// Redistribution and use in source and synthezised forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// Redistributions of source code must retain the above copyright notice,
// this list of conditions and the following disclaimer.
//
// Redistributions in synthesized form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// Neither the name of the author nor the names of other contributors may
// be used to endorse or promote products derived from this software without
// specific prior written permission.
//
// THIS CODE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// You are responsible for any legal issues arising from your use of this code.
//

#include "SdFat.h"
#include <SPI.h>
#include <Wire.h>
#include "pins.h"

// spi version 1.10
const unsigned char version[4] = "110"; 

// the OSD screen width in chars
#define SCREEN_WIDTH_IN_CHARS 32

//  the OSD screen height in chars
#define MENU_MAX_LINES 8

// the keycodes produced by the core (lower 5bits)
#define KEY_UP  30
#define KEY_DOW 29
#define KEY_LFT 27 
#define KEY_RGT 23 
#define KEY_RET 15
#define KEY_NOTHING 31
#define KEY_A   0  
#define KEY_B   1   
#define KEY_C   2   
#define KEY_D   3  
#define KEY_E   4   
#define KEY_F   5   
#define KEY_G   6  
#define KEY_H   7  
#define KEY_I   8  
#define KEY_J   9   
#define KEY_K   10  
#define KEY_L   11  
#define KEY_M   12  
#define KEY_N   13  
#define KEY_O   14 
#define KEY_P   16  
#define KEY_Q   17  
#define KEY_R   18 
#define KEY_S   19  
#define KEY_T   20  
#define KEY_U   21 
#define KEY_V   22
#define KEY_W   24  
#define KEY_X   25  
#define KEY_Y   26  
#define KEY_Z   28 

// events produced by the core
#define EVENT_NOTHING 0
#define EVENT_KEYPRESS 1

// the maximum number of files per directory that can be sorted (the limit is the available memory)
#define MAX_SORTED_FILES 128

// the filename length
#define FILENAME_LEN 64

// the maximum of extensions that can be parsed (separated by "/") (context: filtering files in navigateMenu)
#define MAX_PARSED_EXTENSIONS 10 

// the maximum length of extensions (including ".")
#define MAX_LENGTH_EXTENSION 5

// the maximum of options to be show
#define MAX_OPTIONS 32

// the maximum length of an option title
#define OPTION_LEN  32

// one entry in a directory (may be either a file or directory)
struct SPI_Directory {
  char filename[FILENAME_LEN];
  enum entry_type { none=0,file=1, dir=2} entry_type;
};

unsigned char core_mod = 0;

unsigned char transfer_index =0;

char core_name[32];
char dat_name[32];

SdFat sd1(1);

SdFile  file;
SdFile  entry;

bool CORE_ok = true;

char sd_buffer[1024];
unsigned char ret;
unsigned long menu_opened;

unsigned char option_sel[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
unsigned char option_num[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

unsigned char key;
unsigned char cmd;

// only the 5 lower bit are keys
unsigned char last_key = 31; 
unsigned char last_cmd = 1;
    
unsigned char last_ret;
unsigned char cur_select = 0;

char file_selected[64];

// TODO: TRY TO USE endsWith, defined in fileSort (should it be there??)
bool isExtension( char* filename, char* extension ) {
  int8_t len = strlen(filename);
  char ext[ 5 ];
  char file[ 64 ];
  bool result;
  char temp[ 64 ];
  char *token;
       
  strcpy( file, filename );  
  strcpy( temp, extension );  
        
  token = strtok( temp, "/" );
    
  while( token ) {
    strcpy( ext, token );  
  
    // Serial.print ("isExtension:");
    // Serial.println (token);
        
    if ( strstr( strlwr( file + ( len - 4 ) ), strlwr( ext ) ) ) {
      return true;
    } 
    
    token = strtok( NULL, "/" );
  }
    
  return false;
}

unsigned char codeToNumber (unsigned char code) {
    if ( code >= '0' && code <= '9' ) return code - 48;
    if ( code >= 'A' && code <= 'Z' ) return code - 55;

    return code;
}


void SendStatusWord (void)
{
    //generate the Status Word
    unsigned int statusWord = 0;
    for ( int j = 0; j < 32; j++ ) {
    
      statusWord += ( option_sel[ j ] & 0x07 ) << ( option_sel[ j ] >> 3 );    
    }
    
    //Serial.print( "statusWord " );
    //Serial.println( statusWord, BIN );
    SPI_SELECTED();
    
    // cmd to send the status data
    spi_osd_cmd_cont( 0x15 ); 
    spi32( statusWord );
    spi8( core_mod );
    SPI_DESELECTED();
}


/**
 * This is the core/file selection menu
 * 
 * arguments:
 * ----------
 * 
 * char *extension => string with preferred file extension for cores (separated by "/", no ".")
 * bool canAbort => allow aborting by pressing F12?
 * bool filter => shows only files matching the extension(s)?
 * bool showDir => shows directories?
 * bool showExtensions => shows the file extensions?
 * 
 * return value
 * ------------
 * 
 * bool => true     when a core is selected (also sets the global file_selected, needed elsewhere)
 *      => false    when user request abort (pressed F12)
 */
bool navigateMenu( char* extension, bool canAbort, bool filter, bool showDir, bool showExtensions ) {
  int index=0;
  int event;
  unsigned char total_files = 0;
  unsigned char page = 0;
  unsigned char last_page = 0;
  bool selected = false;
  bool showfile = false;
  bool isDir = false;
  bool isSelectedDir = false;
  bool insideDir = false;
  
  char ext[5];
  
  // filtered extensions, with "."
  char extensions[ MAX_PARSED_EXTENSIONS ][ MAX_LENGTH_EXTENSION ]; 
  
  unsigned char cur_line = 0;
  
  unsigned char total_show = 0;
  
  showDir = true;
  
  cur_select=0;
  
  Serial.print( "navigateMenu - extension: " );
  Serial.print( extension );
  Serial.print( "| canAbort " );
  Serial.print( canAbort ? "true" : "false" );
  Serial.print( "| filter " );
  Serial.print( filter ? "true" : "false" );
  Serial.print( "| showDir" );
  Serial.print( showDir ? "true" : "false" );
  Serial.print( "| showExtensions " );
  Serial.println( showExtensions ? "true" : "false" );

  // prepare extensions
  int totalExtensions;
  prepareExtensions( extension, extensions, &totalExtensions );
  
  // sort files (TODO check configuration/file limit to avoid slowness)
  int totalFiles = 0;
  SPI_Directory orderedFiles[ MAX_SORTED_FILES ];
  sortFiles( sd1, orderedFiles, &totalFiles, extensions, totalExtensions, showDir );

  // if totalFiles is exactly MAX_SORTED_FILES, then some files may be not shown. Let's inform the user
  if ( totalFiles == MAX_SORTED_FILES ) {
    char errorMsg[33];
    sprintf(errorMsg,">= %d files in root directory.",MAX_SORTED_FILES);
    errorScreen( errorMsg );
    waitKeyPress();
  }

  // currently selected page
  int currentPage = 0;
  // currently selected line
  int currentLine = 0;
  // how much time (ms) passed since the  last time the selected file has rolled
  int idleTime = millis();
  // how many characters has been currently rolled
  int rolling_offset = 0; 
  while( ! selected )
  {
    int j;
    
    // show files on screen
    show_files( orderedFiles, extensions, totalExtensions, showExtensions, currentPage, currentLine, totalFiles, &rolling_offset, &idleTime ); 
    
    // read keyboard and act
    event = readKeyboard( &key, &cmd );

    if ( event == EVENT_KEYPRESS ) {
      idleTime = millis( );
      rolling_offset = 0;
      switch( key ){
        case KEY_UP: // up
          currentLine --;
          if ( currentLine < 0 ) {
            currentLine = totalFiles - 1;
          }
          break;
        case KEY_DOW: // down
          currentLine ++;
          if ( currentLine >= totalFiles ) {
            currentLine = 0;
          }
          break;          
        case KEY_LFT: // left
          currentLine -= MENU_MAX_LINES;
          if ( currentLine < 0 ) {
            currentLine = totalFiles + currentLine ;
          }
          break;
        case KEY_RGT: // right
          currentLine += MENU_MAX_LINES;
          if ( currentLine >= totalFiles ) {
            currentLine -= totalFiles;
          }
          break;          
          
        case KEY_A:
        case KEY_B:
        case KEY_C:
        case KEY_D:
        case KEY_E:
        case KEY_F:
        case KEY_G:
        case KEY_H:
        case KEY_I:
        case KEY_J:
        case KEY_K:
        case KEY_L:
        case KEY_M:
        case KEY_N:
        case KEY_O:
        case KEY_P:
        case KEY_Q:
        case KEY_R:
        case KEY_S:
        case KEY_T:
        case KEY_U:
        case KEY_V:
        case KEY_W:
        case KEY_X:
        case KEY_Y:
        case KEY_Z:
        //case KEY_0:
          currentLine = findFirstByInitial( mapKeyToChar( key ), totalFiles, currentLine, orderedFiles );
          break;
          
        case KEY_RET: // enter
          Serial.print( "Pressed enter... " );
          Serial.print( orderedFiles[ currentLine ].entry_type );
          Serial.println( " yep" );
          
          if ( orderedFiles[ currentLine ].entry_type == SPI_Directory::dir ) { 
            
            Serial.print( "Changing to directory " );
            Serial.println( orderedFiles[ currentLine ].filename );
            if ( strcmp( orderedFiles[ currentLine ].filename, ".." ) == 0 ) {
              sd1.chdir( );
            } else {
              sd1.chdir( orderedFiles[ currentLine ].filename );
            }
                      
            // retrieve files from this dir
            totalFiles = 0;
            sortFiles( sd1, orderedFiles, &totalFiles, extensions, totalExtensions, showDir );

            // if totalFIles is exactly MAX_SORTED_FILES, then some files may be not shown. Let's inform the user
            if ( totalFiles == MAX_SORTED_FILES ) {
              char errorMsg[ 33 ];
              sprintf( errorMsg, ">= %d files in this directory.", MAX_SORTED_FILES );
              errorScreen( errorMsg );
              waitKeyPress( );
            }
            
            currentPage = 0;
            currentLine = 0;
            
          } else {
            // not a dir; then it is a file
            selected=true; // TODO after putting "return true" here, this can be removed I guess
            strcpy( file_selected, orderedFiles[ currentLine ].filename );
            return true;
          }
            break;
          case KEY_NOTHING: // nothing was pressed
            break;
          default:
            Serial.print("Unrecognized key pressed: ");
            Serial.println(key);
            break;
      }

      //F12 - abort
      if ( ( cmd == 0x01 || cmd == 0x02 || cmd == 0x03 ) && canAbort ) { 
        Serial.println("Aborting");
        return false;  
      }
            
      // recalculate page
      if ( currentPage != currentLine / MENU_MAX_LINES ) {
        currentPage = currentLine / MENU_MAX_LINES;
        OsdClear( );
      }
      
    }// end if events
    
  }// end while
}

void waitACK (void)
{
  // set second SPI on PA12-PA15
  SPI.setModule(2);
  
  
  SPI_DESELECTED(); 
  
  // syncronize and clear the SPI buffers
  // Serial.println ("waitACK");
  
  //wait for command acknoledge
  while( ret != 'K' ) {
    SPI_SELECTED();  
    // clear the SPI buffers
    ret = SPI.transfer( 0 );
    
    // wait a little to receive the last bit
    delay(100);
    
     
    SPI_DESELECTED(); 
     
    if( last_ret != ret ) {
      last_ret = ret;
    }
    
    // Serial.print ("\n");
    // Serial.print (ret);
    // Serial.print ("=");
    // Serial.print ((char)ret);
      
  }
  
  SPI.setModule(1);
}


bool waitSlaveCommand( void ) {
    unsigned char cmd;
    unsigned int char_count = 0;

    //Serial.println( "\nwaitSlaveCommand" );

    // set second SPI on PA12-PA15
    SPI.setModule( SPI_FPGA );
    SPI_DESELECTED();

    //SS clear to send a comand
    SPI_SELECTED();
  
    //read data from slave
    ret = SPI.transfer(0x10); 
          
    //wait for commands
    while( true ) {

        cmd = SPI.transfer( 0x00 ); //Dummy data, just to get the response 
            
        //  Serial.print ("\n cmd: ");
        //  Serial.println (cmd, BIN);
            
        cmd = ( cmd & 0xe0 ) >> 5; //just the 3 msb are the command

        // IF PRESSED F12
        if ( cmd == 0x01 || cmd == 0x02  || cmd == 0x03 ) {
            char_count = 0;

            OsdClear();
            EnableOsdSPI();

            // let's ask if we need a specific file or need to show the menu
            // command 0x14 - get STR_CONFIG
            ret = SPI.transfer(0x14); 

            while(ret != 0) {

                // delay(50);
                ret = SPI.transfer(0x00); //get one char at time
                sd_buffer[char_count] = ret; //use the sd_buffer as a general buffer

                // Serial.print ("\n command 0x14 reply:");
                // Serial.print (ret);
                // Serial.print ("=");
                Serial.print( ( char ) ret );
                char_count++;
                    
            }
            // Serial.println (" ");


            // read an external ".INI", if exists
            // strcpy(core_name,"robotron"); //debug
            strcpy( file_selected, core_name );
            strcat( file_selected, ".ini" );
            file.open( file_selected );

            Serial.print( file_selected );

            if ( file.isOpen( ) ) {
                  
                char line[128];
                char *token; 
                unsigned int n;
                unsigned int count;

                Serial.println( " opened" );
                char_count-- ;

                while( ( n = file.fgets( line, sizeof( line ) ) ) > 0 ) {
  
                    int i=0;
                    for ( i = 0; i < 128; i++ ) {
                        //clip the line at the CR or LF
                        if( line[i] == 0x0d || line[i] == 0x0a) { 
                           line[i] = '\0';
                           break;
                        }   
                    }

                    token = strtok( line, "=" );

                    //CONF option, add a config string to the menu
                    if ( strcmp( strlwr( token ), "conf" ) == 0 ) {

                        token = strtok( NULL, "=" );
                        //Serial.println(token);

                        int i=0; 

                        sd_buffer[char_count] = ';';
                        char_count++;

                        while( token[i] ) {
                            //remove quotes and CR LF
                            if( token[i] != '"' ) { // && token[i]!= 0x0d && token[i]!= 0x0a) 
                                sd_buffer[char_count] = token[i];
                                char_count++;
                            }
                            i++;
                        }

                    } else if( strcmp( strlwr( token ), "mod" ) == 0 ) {

                        //MOD option, to assign a hardware number (core variant)
                        token = strtok(NULL, "=");
                        Serial.print("MOD = ");
                        Serial.println(atoi(token),HEX);
                        core_mod = atoi(token);

                    } else if( strcmp( strlwr( token ), "name" ) == 0 ) {
                        //NAME option, to force a CORE_NAME (different than the loaded .mc2)
                        token = strtok(NULL, "=");
                        strcpy(dat_name,token);
                        Serial.print("|");
                        Serial.print(token);
                        Serial.println("|");
                    } else if ( strcmp( strlwr( token ), "options" ) == 0 ) {
                        int i = 0;
                        token = strtok( NULL, "," );
                        while( token ) {
                            /*   Serial.print("option ");
                            Serial.print(i);
                            Serial.print(" = ");
                            Serial.println(token);
                            */
                            option_sel[i] = atoi( token );
                            token = strtok( NULL, "," );
                            i++;
                        }

                    } // end if strcmp(strlw...)  
                } // end while
                sd_buffer[char_count] = ';';
                sd_buffer[++char_count] = '\0';

                Serial.print("final:");
                Serial.print(sd_buffer);
                Serial.println("|");
                     
            } else {
                Serial.println(" - no file");
            }
            file.close();
            // end .INI parse process

            // add the "load other" at the end of Options menu
            char_count--;
            char temp[22] = { ";R,Load Other Core..." };
            for ( int i = 0; i < sizeof( temp ); i ++ ) {
              sd_buffer[ char_count ++ ] = temp[i];
            }

            sd_buffer[ char_count ] = ';';
            sd_buffer[ ++ char_count ] = '\0';

            Serial.print( "After 'other':" );
            Serial.print( sd_buffer );
            Serial.println( "|" );

            //check if we have a CONF_STR
            if( char_count > 1 && ( cmd == 0x01 || cmd == 0x03 ) ) {
                char temp[64];

                Serial.println("CMD 0x01");

                // command 0x55 - UIO_FILE_INDEX;
                SPI.transfer(0x55); 
                // index 0 - ROM
                SPI.transfer(0x00); 

                // slave deselected
                SPI_DESELECTED(); 
                // slave selected
                SPI_SELECTED(); 

                //initial data pump only when cmd=0x01 
                if( sd_buffer[0] == 'P' && sd_buffer[1] == ',' && cmd == 0x01 ) {
                    //datapump only
                    int strpos;

                    char *token = strtok(sd_buffer, ",");
                    token = strtok(NULL, ",");
                    token = strtok(token, ";");

                    Serial.print ("\ntoken pump:");
                    Serial.print (token);

                    // test to see if the two strings are equal
                    if ( strcmp( token, "CORE_NAME.dat" )  == 0 )  {
                        strcpy( file_selected, dat_name );
                        strcat( file_selected, ".dat" );
                    } else {
                        strcpy( file_selected, token );
                    }
                    transfer_index = 0; //index for the ROM
                    dataPump();
                    SendStatusWord(); //update the core with the read options

                } else {
                    //we have an "options" menu
                    SendStatusWord(); //update the core with the read options
                    // is it a new core load?
                    if ( ! navigateOptions() ) {
                      return false; 
                    }
                    SaveIni(); //save the options on SD card
                    OSDVisible(false);
                }
                     
            } else {
                //we dont have a STR_CONF, so show the menu or CMD = 0x02
                Serial.println( "CMD 0x02" );
                Serial.println( "we dont have a STR_CONF, so show the menu" );
                SPI_DESELECTED(); 

                //show the menu to navigate
                OSDVisible( true );

                if( navigateMenu( "", true, false, false, false ) ) {
                    
                    SPI_DESELECTED(); 
                    // command 0x55 - UIO_FILE_INDEX;
                    spi_osd_cmd_cont( 0x55 );
                    // index as 0x01 for Sinclair QL  
                    SPI.transfer( 0x01 ); 

                    
                    SPI_DESELECTED(); 
                    //slave selected
                    SPI_SELECTED(); 

                    Serial.println( "we will execute a data pump 2!" );     
                    dataPump( );
                }
      
                OSDVisible( false );
            }
                
            break; //ok!
        } else if ( cmd == 0x07 ) {
            // Serial.println("CMD 0x07");
            break;
        }
          
    }
   
    DisableOsdSPI( );
    return true;
}

void SaveIni(void) {
    char temp_file[9] = { "temp.ini" };
    SdFile  tfile;

    strcpy(file_selected,core_name);
    strcat(file_selected,".ini");

    if ( strncmp( file_selected, ".ini", 4 ) == 0 ) 
    {
       //no core name, so don't save anything
        Serial.print( "Aborting SaveIni" );
        return;
    }

    if ( ! sd1.exists( file_selected ) ) { 
        file.open( file_selected, FILE_WRITE );
    }
    else {
        file.open( file_selected );
    }      

    tfile.open( temp_file, O_WRITE | O_CREAT | O_TRUNC );

    if ( file.isOpen( ) ) {
        char line[128];
        char *token; 
        unsigned int n;

        while ( ( n = file.fgets( line, sizeof( line ) ) ) > 0) {
            if( strncmp( line, "OPTIONS=", 8 ) != 0 ) {
                tfile.print(line);
            }
        }   

        //add the options to the file
        tfile.print( "OPTIONS=" );
        for ( int i = 0; i < 32; i++ ) {
            tfile.print( option_sel[i], DEC );
            if ( i < 31 ) {
                tfile.print( "," );
            }
        }
        tfile.println( "" );
    }

    file.close( );
    tfile.close( );

    sd1.remove( file_selected );
    sd1.rename( temp_file, file_selected );

    Serial.print( "SaveIni: " );
    Serial.println( file_selected );
}

uint16_t crc16_update( uint16_t crc, uint8_t a ) {
    int i;

    crc ^= a;
    for( i = 0; i < 8; ++i ) {
        if( crc & 1 )
            crc = ( crc >> 1 ) ^ 0xA001;
        else
            crc = ( crc >> 1 );
    }
    return crc;
 }

uint16_t crc16_CCITT( uint16_t  crc, uint8_t data ) {
    uint8_t i;

    crc = crc ^ ( ( uint16_t ) data << 8 );
    for( i = 0; i < 8; i++ ) {
        if( crc & 0x8000 )
            crc = ( crc << 1 ) ^ 0x1021;
        else
            crc <<= 1;
        }
    return crc;
}


uint16_t checksum_16( uint16_t  total, uint8_t data ) {
    total = total + data;
    return total;
}
 
void dataPump( void ) {
    char          buffer_temp[6];
    unsigned long file_size;
    unsigned long to_read;
    unsigned int  read_count     = 0;
    unsigned char percent        = 0;
    unsigned long loaded         = 0;
    uint16_t      crc_read       = 0;
    uint16_t      crc_write      = 0;
    int           state          = LOW;
    unsigned char c              = 0;
    unsigned char digit, val;

    strlwr( file_selected );

    memcpy( buffer_temp, & file_selected[ strlen( file_selected ) - 4 ], 4 );
    buffer_temp[4] = '\0';

    file.open( file_selected );
    file_size = file.fileSize( );
        
    read_count = file_size / sizeof( sd_buffer );
    if ( file_size % sizeof( sd_buffer ) != 0 ) {
        read_count++;     
    }
                 
    SPI.setModule( SPI_FPGA );

    
    SPI_DESELECTED(); 

    Serial.print( "\ndata pump : " );
    Serial.println( file_selected );

    Serial.print( "file ext : " );
    Serial.println( ( char* ) buffer_temp );

    Serial.print( "file size : " );
    Serial.println( file_size );

    Serial.print( "read_count : " );
    Serial.println( read_count );

    Serial.print( "index : " );
    Serial.println( transfer_index );

    SPI_SELECTED();

    //send the transfer_index
    // command 0x55 - UIO_FILE_INDEX;
    spi_osd_cmd_cont( 0x55 ); 
    
    // index 0 - ROM, 1 = drive 1, 2 = drive 2
    spi8( transfer_index ); 
    SPI_DESELECTED();

    // config buffer - 16 bytes
    spi_osd_cmd_cont( 0x60 ); //cmd to fill the config buffer

    // spi functions transfer the MSB byte first
    // 4 bytes for file size
    spi32( file_size ); 

    //3 bytes for extension
    spi8( buffer_temp[ 1 ] );
    spi8( buffer_temp[ 2 ] );
    spi8( buffer_temp[ 3 ] );
    spi8( 0xFF );

    spi8( 0xFF );
    spi8( 0xFF );
    spi8( 0xFF );
    spi8( 0xFF );

    spi8( 0xFF );

    //last bytes as STM version
    spi8( version[2] - '0' );
    spi8( version[1] - '0' );
    spi8( version[0] - '0' );

    DisableOsdSPI( ); 

    // select the SD Card SPI 
    SPI.setModule( SPI_SD ); 

    unsigned int startTime = millis();
    unsigned int duration = 0;
        
    for (int k = 1; k < read_count + 1; k++ ) 
    { 
        unsigned char val;
    
        to_read = ( file_size >= ( sizeof( sd_buffer ) * k ) ) ? sizeof( sd_buffer ) : file_size - loaded + 1;
        val = file.read( sd_buffer, to_read );

        // select the second SPI (connected on FPGA)
        SPI.setModule( SPI_FPGA );

        //SS clear to send a comand 
        SPI_SELECTED();  
        ret = SPI.transfer( 0x61 ); //start the data pump to slave

/*     esse faz o SF alpha em 29 segundos
        for ( int f=0; f < to_read; f+=4 ) {
            //check for a WAIT state
            if ( ! CHECK_WAIT() ) {
                Serial.println( "waiting SPI" );
                delay( 1 );
            } 
            
            SPI.write( sd_buffer[ f     ]);
            SPI.write( sd_buffer[ f + 1 ]);
            SPI.write( sd_buffer[ f + 2 ]);
            SPI.write( sd_buffer[ f + 3 ]);
                
        }
        */

        // substitui o bloco acima. SF alpha em 22 segundos
        SPI.write(sd_buffer, to_read);


        
        // end the pumped block
        SPI_DESELECTED();                

        loaded += to_read;
        percent = loaded * 100 / file_size;

        digit = 0;
        val = percent;
        while ( val > 9 ) {
            val -= 10;
            digit++ ;
        }

        buffer_temp[0] = ' ';
        buffer_temp[1] = '0' + digit;
        buffer_temp[2] = '0' + val;
        buffer_temp[3] = '%';
        buffer_temp[4] = ' ';
        buffer_temp[5] = '\0';

        OsdWriteOffset( 6, "            Loading", 0 , 0, 0, 0 );
        OSD_progressBar( 7, buffer_temp, percent );

        // select the SD Card SPI   
        SPI.setModule( SPI_SD ); 

        state++;

        //led off
        digitalWrite( PIN_LED, state >> 2 & 1 ); 
    }

    //led off
    digitalWrite( PIN_LED, HIGH ); 
    file.close();
      
    // CLEAR THE LOADING MESSAGE
    OsdWriteOffset( 6, " ", 0 , 0, 0, 0 );
    OsdWriteOffset( 7, " ", 0 , 0, 0, 0 );

    Serial.println( "end data" );

    duration = millis() - startTime;
    Serial.print( "data pump in milliseconds: " );
    Serial.println( duration );
    
    crc_read = 0;
    crc_write = 0;
        
    // end the data pump
    spi_osd_cmd_cont( 0x62 ); 

    // SS end sequence
    SPI_DESELECTED(); 
}

//   JTAG
void setupJTAG( ) {

    pinMode( PIN_TCK, OUTPUT );
    pinMode( PIN_TDO, INPUT_PULLUP );
    pinMode( PIN_TMS, OUTPUT );
    pinMode( PIN_TDI, OUTPUT );
    digitalWrite( PIN_TCK, LOW );
    digitalWrite( PIN_TMS, LOW );
    digitalWrite( PIN_TDI, LOW );
}

void releaseJTAG( ) {

    pinMode( PIN_TCK, INPUT_PULLUP );
    pinMode( PIN_TDO, INPUT_PULLUP );
    pinMode( PIN_TMS, INPUT_PULLUP );
    pinMode( PIN_TDI, INPUT_PULLUP );
}

void error( ) {
    Serial.println("JTAG");
    Serial.println("ERROR!!!!");
}

void program_FPGA( ) {
    unsigned long bitcount = 0;
    bool last = false;
    int n = 0; 

    Serial.print( "Programming" );    
    JTAG_PREprogram(); 

    int mark_pos = 0;
    int total = file.fileSize();
    int divisor = total / 32; 
    int state = LOW;

    unsigned long file_size;
    unsigned int read_count = 0;
    unsigned char val;
    unsigned long to_read;
    unsigned long loaded = 0;
    int value;

    file_size = file.fileSize();
    read_count = file_size / sizeof( sd_buffer );
    if ( file_size % sizeof( sd_buffer ) != 0 ) {
      read_count ++;
    }

    // no Interrupts()
    for ( int k = 1; k < read_count + 1; k++ ) { 
      to_read = ( file_size >= ( sizeof( sd_buffer ) * k ) ) ? sizeof( sd_buffer ): file_size - loaded + 1;
      val = file.read( sd_buffer, to_read );
      for( int f = 0; f < to_read; f ++ ) {
        
        if ( bitcount % divisor == 0 ) {
            Serial.print( "*" );
            state = ! state;
            digitalWrite( PIN_LED, state );
        }
        bitcount++ ;
  
        for( n = 0; n <= 7; n++ )
        {
          if ( ( sd_buffer[ f ] >> n ) & 0x01 ) {
            // tdipin = 1
            GPIOB->regs->ODR |= 2;
          }
          else {
            // tdipin = 0
            GPIOB->regs->ODR &= ~(2);  
          }
          
           JTAG_clock();
        }
      }
      loaded += to_read;
  }

  /* AKL (Version1.7): Dump additional 16 bytes of 0xFF at the end of the RBF file */
  GPIOB->regs->ODR |= 1;
  for ( n = 0; n < 127; n++ ) {
    GPIOB->regs->ODR |= 1;
    GPIOB->regs->ODR &= ~(1);    
  }

  digitalWrite( PIN_TDI, HIGH ); 
  digitalWrite( PIN_TMS, HIGH );
  JTAG_clock( );

  Serial.println( "" );
  Serial.print( "Programmed " );
  Serial.print( bitcount );
  Serial.println( " bytes" );

  file.close();

  JTAG_POSprogram();
}


void initOSD(void)
{
    EnableOsdSPI();  
    OsdClear();
    OSDVisible(true);
}

void removeOSD(void)
{
    EnableOsdSPI();  
    OsdClear();
    OSDVisible(false);
    DisableOsdSPI();
}

void initialData( void ) {
    // config buffer - 16 bytes
    spi_osd_cmd_cont(0x60); //cmd to fill the config buffer

    // TODO - make it a for?
    spi8(0xFF);
    spi8(0xFF);
    spi8(0xFF);
    spi8(0xFF);

    spi8(0xFF);
    spi8(0xFF);
    spi8(0xFF);
    spi8(0xFF);

    spi8(0xFF);
    spi8(0xFF);
    spi8(0xFF);
    spi8(0xFF);

    spi8(0xFF);

    //last bytes as STM version
    spi8(version[2] - '0');
    spi8(version[1] - '0');
    spi8(version[0] - '0');

    DisableOsdSPI(); 
}

/**
 * prepare hardware
 *
 **/
void setup( void )
{
    //Initialize serial and wait for port to open:
    //Serial.begin( 115200 );
 
    while ( ! Serial ) {
      // wait for serial port to connect. Needed for native USB port only
      delay( 100 ); 
    }
    
    Serial.println( "Serial ok..." );

    Serial.print( "Initializing SPI version " );
    Serial.println( ( char* ) version );

    pinMode( PIN_nWAIT, INPUT_PULLUP );
    pinMode( PIN_LED, OUTPUT );

    
    // configure NSS pin
    pinMode( PIN_NSS, OUTPUT ); 

    // set second SPI on PA12-PA15
    SPI.setModule( SPI_FPGA );

    //setup SPI interface
    SPI_DESELECTED(); 
    SPI.begin( );
    SPI.setClockDivider( SPI_CLOCK_DIV2 ); //72mhz / 2 = SPI_FPGA @ 36Mhz 
    SPI_DESELECTED(); 
        
    waitACK( );
    initialData( );

    // select the SD Card SPI
    SPI.setModule( SPI_SD ); 

    if ( ! sd1.begin( PIN_CSSD, SD_SCK_MHZ( 50 ) ) ) {

        Serial.println( "SD Card initialization failed!" );
        initOSD( );
        OsdWriteOffset( 3, "          No SD Card!!! ", 1, 0, 0, 0 ); 

        //hold until a SD is inserted
        while ( ! sd1.begin( PIN_CSSD, SD_SCK_MHZ( 50 ) ) ) {
            NULL;
        }
        strcpy( file_selected, "core." EXTENSION ); // REFACTOR
        if ( ! sd1.exists( file_selected ) ) 
        { 
            Serial.println( "core." EXTENSION " not found" );
            CORE_ok = false;
        }
        removeOSD( );
    } 
    
    strcpy( file_selected, "core." EXTENSION ); // REFACTOR
    if ( ! sd1.exists( file_selected ) ) 
    { 
        Serial.println( "core." EXTENSION " not found" );
        CORE_ok = false;
    }
      
    Serial.println("SD Card initialization done.");

}// end of setup



void menuLoadNewCore (bool splash)
{
    EnableOsdSPI();  
    OsdClear();
    OSDVisible( true );

    if ( splash ) {
      splashScreen();
    }

    //if we receive a command, go to the command state (to make de development easier)
    if ( ! navigateMenu( EXTENSION, true, true, false, false ) )  {
      
      OSDVisible( false );
      
      do {
        waitACK();
      } while( waitSlaveCommand( ) );
      
    }
    
    CORE_ok = true;
}


/**
 * Main loop - manages the core menu and the application menu
**/
void loop ( void )
{
  Serial.println( "loop" );

  //if we have a core.mc2, lets transfer, without a menu
  if  ( CORE_ok ) {
    // Save the core name to use later inside the core
    for( int i = 0 ; i < sizeof( core_name ); i++ ) {
      core_name[i] = '\0';
    }
    strncpy ( core_name, file_selected, strlen( file_selected ) - 4 ); 
    strcpy ( dat_name, core_name );

    Serial.print ( "core name:" ); 
    Serial.println( core_name );

    file.open( file_selected );

    //led off
    digitalWrite( PIN_LED, HIGH ); 

    // wait for the FPGA power on
    delay( 300 );                       
    setupJTAG( );
    if ( JTAG_scan( ) == 0 ) {
      unsigned int startTime = millis();
      unsigned int duration = 0;
      program_FPGA( );
      duration = millis() - startTime;
      Serial.print( "programming time: " );
      Serial.print( duration );
      Serial.println( "ms" );
    }
    releaseJTAG();

    Serial.println( "OK, finished" );

    //loop forever waiting commands
    do {
      waitACK( );
    } while( waitSlaveCommand( ) );
  }
  else 
  {
    menuLoadNewCore( true );
  }
        
}  // end of loop
