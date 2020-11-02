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

#include "charrom.h"

#define MM1_OSDCMDWRITE    0x20      // OSD write video data command
#define MM1_OSDCMDENABLE   0x41      // OSD enable command
#define MM1_OSDCMDDISABLE  0x40      // OSD disable command

#define OSDNLINE         8           // number of lines of OSD
#define OSDLINELEN       256         // single line length in bytes

#define OSD_ARROW_LEFT 1
#define OSD_ARROW_RIGHT 2


void spi_osd_cmd_cont(unsigned char cmd) 
{
  EnableOsdSPI();
  SPI.transfer(cmd);
}

void spi_n(unsigned char val, unsigned short cnt) 
{
  while(cnt--) 
    SPI.transfer(val);
}

void spi8(unsigned char parm) 
{
  SPI.transfer(parm);
}

void spi16(unsigned short parm) 
{
  SPI.transfer(parm >> 8);
  SPI.transfer(parm >> 0);
}

void spi24(unsigned long parm) 
{
  SPI.transfer(parm >> 16);
  SPI.transfer(parm >> 8);
  SPI.transfer(parm >> 0);
}

void spi32(unsigned long parm) 
{
  SPI.transfer(parm >> 24);
  SPI.transfer(parm >> 16);
  SPI.transfer(parm >> 8);
  SPI.transfer(parm >> 0);
}

void EnableOsdSPI()
{
     SPI.setModule(2); // select the second SPI (connected on FPGA) 
     SPI_SELECTED(); // slave active
}

void DisableOsdSPI()
{
    SPI_DESELECTED(); //slave deselected
    SPI.setModule(1); // select the SD Card SPI
}

void OSDVisible (bool visible)
{
        if (visible)
        {
          spi_osd_cmd_cont(0x41);
          menu_opened = millis();
        }
        else
        {
          spi_osd_cmd_cont(0x40);
        }

        DisableOsdSPI();
}



void OsdClear(void)
{

     spi_osd_cmd_cont(0x20);


    // clear buffer
    spi_n(0x00, OSDLINELEN * OSDNLINE);

  DisableOsdSPI();

}

void OsdWrite(unsigned char n, char *s, unsigned char invert, unsigned char stipple, unsigned char arrow)
{
    OsdWriteOffset(n,s,invert,stipple,0, arrow);
}

// write a null-terminated string <s> to the OSD buffer starting at line <n>
void OsdWriteOffset(unsigned char n, char *s, unsigned char invert, unsigned char stipple,char offset, unsigned char arrow)
{
      unsigned short i;
      unsigned char b;
      const unsigned char *p;
      unsigned char stipplemask=0xff;
      int linelimit=OSDLINELEN-1;
      int arrowmask=arrow;

      // Serial.print("OsdWriteOffset ");
      //Serial.println(s);
      
      if(n==7 && (arrow & OSD_ARROW_RIGHT))
      {
          linelimit-=22;
      }
      
      if(stipple) 
      {
            stipplemask=0x55;
            stipple=0xff;
      } 
      else
      {
            stipple=0;
      }
      
      // select buffer and line to write to
      spi_osd_cmd_cont(MM1_OSDCMDWRITE | n);

      
      if(invert) invert=255;
      
      i = 0;
      
      // send all characters in string to OSD
      while (1) 
      {
        
         /* if(i==0) 
          {     
                // Render sidestripe
                unsigned char j;
          
                p = &titlebuffer[(7-n)*8];
          
                spi16(0xffff);  // left white border
          
                for(j=0;j<8;j++) spi_n(255^*p++, 2);
          
                spi16(0xffff);  // right white border
                spi16(0x0000);  // blue gap
                i += 22;
          } 
          else*/ if(n==7 && (arrowmask & OSD_ARROW_LEFT)) // Draw initial arrow
          { 
                
                unsigned char b;
          
                spi24(0x00);
                
                p = &charfont[0x10][0];
                
                for(b=0;b<8;b++) spi8(*p++<<offset);
                
                p = &charfont[0x14][0];
                
                for(b=0;b<8;b++) spi8(*p++<<offset);
                
                spi24(0x00);
                spi_n(invert, 2);
                i+=24;
                arrowmask &= ~OSD_ARROW_LEFT;
                
                if(*s++ == 0) break;  // Skip 3 characters, to keep alignent the same.
                if(*s++ == 0) break;
                if(*s++ == 0) break;
          } 
          else 
          {
            
                b = *s++;
                
                if (b == 0)  break; // end of string
           
                
                else if (b == 0x0d || b == 0x0a) // cariage return / linefeed, go to next line
                { 
                    // increment line counter
                    if (++n >= linelimit) n = 0;
                  
                    // send new line number to OSD
                    DisableOsdSPI();
                    
                    spi_osd_cmd_cont(MM1_OSDCMDWRITE | n);
                }
                else if(i<(linelimit-8)) // normal character
                { 
                      unsigned char c;
                      p = &charfont[b][0];
                      
                     // if (b==0x41) Serial.println("----");
                     
                      for( c=0; c < 8; c++ ) //character width
                      {
                        //  if (b==0x41) Serial.println(*p);
                        
                            spi8(((*p<<offset)&stipplemask)^invert);
                            p++;
                            stipplemask^=stipple;
                      }
                      
                      i += 8;
                      
                 }
          }
      }
    
      for (; i < linelimit; i++)  spi8(invert); // clear end of line
       
    
      if(n==7 && (arrowmask & OSD_ARROW_RIGHT)) // Draw final arrow if needed
      { 
            unsigned char c;
            
            spi24(0x00);
            p = &charfont[0x15][0];
            
            for(c=0;c<8;c++) spi8(*p++<<offset);
            
            p = &charfont[0x11][0];
            
            for(c=0;c<8;c++) spi8(*p++<<offset);
            
            spi24(0x00);
            i+=22;
      }
      
      // deselect OSD SPI device
      DisableOsdSPI();
}


// write a null-terminated string <s> to the OSD buffer starting at line <n>
void OSD_progressBar(unsigned char line, char *text, unsigned char percent )
{
    // line : OSD line number (0-7)
    // text : pointer to null-terminated string
    // start : start position (in pixels)
    // width : printed text length in pixels
    // offset : scroll offset in pixels counting from the start of the string (0-7)
    // invert : invertion flag
  
    unsigned long start = 215;
    unsigned long width = 40;
    unsigned long offset = 0;
    unsigned char invert = 0;

    unsigned char count = 0;
    
    const unsigned char *p;
    int i,j;
    
    // select buffer and line to write to
    spi_osd_cmd_cont(MM1_OSDCMDWRITE | line);

    while ( count < start )
    {
        count++;
        
        if (percent < (count*100/214))
        { 
            spi8( 0x00 );
        }
        else
        {
            spi8( 0xff );
        }
    }

    while ( width > 8 ) 
    {
        unsigned char b;
        p = &charfont[*text++][0];
        for(b=0;b<8;b++) spi8(*p++);
        width -= 8;
    }
    
    if (width) 
    {
        p = &charfont[*text++][0];
        while (width--)
          spi8(*p++);
    }
  
    DisableOsdSPI();
}

bool navigateOptions()
{
       //char sd_buffer[1024] {          "S,PRG,Load *.PRG;S,TAP/TZX,Load *.TAP;TC,Play/Stop TAP;OG,Menos sound,Off,On;OH,Mais sound,Off,On;OD,Tape sound,Off,On;O3,Video,PAL,NTSC;O2,CRT with load address,Yes,No;OAB,Scanlines,Off,25%,50%,75%;O45,Enable 8K+ Expansion,Off,8K,16K,24K;O6,Enable 3K Expansion,Off,On;O78,Enable 8k ROM,Off,RO,RW;O9,Audio Filter,On,Off;T0,Reset;T1,Reset with cart unload;." };
      
      int ini = 0;
      int fim=0;
      int cur_line = 0;
      int page = 0;
      int i,j,k;
      int opt_sel =0;
      int total_options = 0;
      int total_show = 0;
      int last_page = 0;

      int event; // keyboard events

      char exts[32];
      char ext[5];

       OsdClear();
      //show the menu to navigate
      OSDVisible(true);
      
      cur_select=0;
      
      while(1)
      {
     



      ini = 0;
      fim = 0;
      opt_sel =0;
      cur_line=0;
      total_options = 0;
      total_show = 0;
      
      while (ini < strlen(sd_buffer))
      {
            for (i=ini; i<strlen(sd_buffer);i++)
            {
                if (sd_buffer[i]==';') 
                {
                    fim = i;
                    break;
                } 
                else
                {
                    fim=strlen(sd_buffer);
                }
            }
        
          
            char temp[128];
            strncpy(temp, sd_buffer + ini, fim-ini);
            temp[fim-ini] = '\0';
            
            ini = fim+1;
            
            Serial.print ("linha:");
            Serial.println (temp);
       
            char *token; 
            token = strtok(temp, ",");

         

            char line[32] = {"                               "};

            if (token[0] == 'S') //it's a LOAD option
            {        
                unsigned char s_index;
                
                if (token[1] != ',') {
                  s_index = codeToNumber(token[1]);
                } else {
                  s_index = 1; //default to "drive 1"
                }
                
                token = strtok(NULL, ",");
            
                if (cur_select==cur_line) 
                {
                  // strcpy(ext,".");
                //   strcat(ext,token); 

                   strcpy(exts, token);
                }
                
                token = strtok(NULL, ",");
                strcpy(line,token);
                line[32] = '\0';
                
                total_options++;

                option_num[opt_sel] = 99;
                option_sel[opt_sel] = s_index; 
                
                if (cur_line >= page*8 && total_show <= 8)
                {
                    OsdWriteOffset(cur_line - (page*8), line, (cur_select==cur_line) , 0, 0, 0);
                    total_show++;
                }
                    
                cur_line++;
                opt_sel++;
            }   
            
            if (token[0] == 'T') //it's an toggle option
            {
                int num_op_token = codeToNumber(token[1]);
                
                token = strtok(NULL, ",");
                strcpy(line,token);
                line[32] = '\0';
                
                total_options++;;

                option_num[opt_sel] = 1;
                option_sel[opt_sel] = (num_op_token<<3); 
                
                if (cur_line >= page*8 && total_show <= 8)
                {
                    OsdWriteOffset(cur_line - (page*8), line, (cur_select==cur_line) , 0, 0, 0);
                    total_show++;
                }
                    
                cur_line++;
                opt_sel++;
            }      
           
            if (token[0] == 'O') //it's an Option for menu
            {

                int num_op_token = codeToNumber(token[1]);
                
                token = strtok(NULL, ",");
                
                for (j=0; j<strlen(token);j++)
                {
                    line[j] = token[j];
                }
                line[j++] = ':';
                line[32] = '\0';
                
                token = strtok(NULL, ",");
                total_options++;;
                unsigned char opt_counter = 0;
                 
                while (token)
                {
                    if ((option_sel[opt_sel] & 0x07) == opt_counter)
                    {
                        for (k=0; k<strlen(token); k++)
                        {
                            line[31-strlen(token)+k] = token[k];
                        }
                      
                    }

                     // Serial.print ("token2 option:");
                    //  Serial.println (token);
                      token = strtok(NULL, ",");
                      
                      opt_counter++;
                      option_num[opt_sel] = opt_counter;
                      option_sel[opt_sel] = (num_op_token<<3) | (option_sel[opt_sel] & 0x07); 
                }
                
             // Serial.print ("opt[");
           //   Serial.print (opt_sel);
            //  Serial.print ("] = ");
            //  Serial.println (opt[opt_sel], BIN);
                  
                if (cur_line >= page*8 && total_show <= 8)
                {
                    OsdWriteOffset(cur_line - (page*8), line, (cur_select==cur_line) , 0, 0, 0);
                    total_show++;
                }
                
                cur_line++;
                opt_sel++;
  
            }
        
        }
        // erase unused lines
        while( total_show < MENU_MAX_LINES ) {
          
          OsdWriteOffset( total_show, "                                ", 0, 0,0,0 );
          total_show++;
        }

      
        // set second SPI on PA12-PA15
        SPI.setModule(2);
        SPI_DESELECTED(); // ensure SS stays high for now

        Serial.println("Waiting keys! 1");
        
        SPI_SELECTED();
        key = SPI.transfer(0x10); //command to read from ps2 keyboad


        Serial.print("last key");

        Serial.print(last_cmd);
        Serial.print(":");
        Serial.println(last_key);
               
        
        // read keyboard and act
        event = readKeyboard(&key,&cmd);
  
        if ( event == EVENT_KEYPRESS ) {
          Serial.println("Key received 1");
  
          if (key == 30) //up
          {
            if (cur_select > 0) cur_select--;
  
             if (cur_select < page*8) 
             {
                cur_select = page*8 - 1;
                page--;
             }
          }
          else if (key == 29) //down
          {
             if (cur_select < total_options-1) cur_select++; 
  
             if (cur_select > ((page+1)*8)-1) page++;
          }
          else if (key == 27) //left
          {
             if (page > 0) page--;  
          }
          else if (key == 23) //right
          {
             if (page < ((total_options-1)/8)) page++; 
          }
          else if (key == 15) //enter
          {
              if (option_num[cur_select] == 99)
              {  
                  transfer_index = option_sel[cur_select];
                  OsdClear();
                  //Serial.print(ext);Serial.println("-----------------------------");
                  if (navigateMenu(exts, true, false,false,true))
                        {
                            
                            SPI_DESELECTED(); //slave deselected
            
                            SPI_SELECTED(); //slave selected
                            
                            Serial.println("we will execute a data pump 3!");     
                            dataPump();
  
                            OSDVisible(false);
                            last_cmd = 1;
                            last_key = 31;
                            return(true);
                        }
              }
              else
              {
                  option_sel[cur_select]++;
                  
                  if ((option_sel[cur_select] & 0x07) == (option_num[cur_select]) && option_num[cur_select] > 1) 
                      option_sel[cur_select] = 0; 
              }
          }
          else if ((cmd == 0x01 || cmd == 0x02 || cmd == 0x03)) //F12 - abort
          {
  
              if (menu_opened + 1000 < millis())
                  return false;  
          }
  
          if (page != last_page)
          {
              last_page = page;
              if (key !=30) cur_select = page*8;
              OsdClear();
          }
        }

/*
        Serial.print("option_num ");
        for (int j=0; j<32; j++)
        {
          Serial.print (option_num[j]);
           Serial.print(",");
        }
        Serial.println(" ");

         Serial.print("option_sel ");
        for (int j=0; j<32; j++)
        {
          Serial.print (option_sel[j]&0x07);
          Serial.print(",");
        }
        Serial.println(" ");

        Serial.print("option_sel ");
        for (int j=0; j<32; j++)
        {
          Serial.print (option_sel[j]);
          Serial.print(",");
        }
        Serial.println(" ");
  */
        SendStatusWord();
        
       
        
      }

       
    
}
