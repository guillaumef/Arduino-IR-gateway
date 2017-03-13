#include <IRremote.h>

/* Mandatory use of 'define', int is not behaving the same */
#define WAIT_LED 13
#define RECV_PIN 11

#define portOfPin(P)\
  (((P)>=0&&(P)<8)?&PORTD:(((P)>7&&(P)<14)?&PORTB:&PORTC))
#define ddrOfPin(P)\
      (((P)>=0&&(P)<8)?&DDRD:(((P)>7&&(P)<14)?&DDRB:&DDRC))
#define pinOfPin(P)\
            (((P)>=0&&(P)<8)?&PIND:(((P)>7&&(P)<14)?&PINB:&PINC))
#define pinIndex(P)((uint8_t)(P>13?P-14:P&7))
#define pinMask(P)((uint8_t)(1<<pinIndex(P)))

#define pinAsInput(P) *(ddrOfPin(P))&=~pinMask(P)
#define pinAsInputPullUp(P) *(ddrOfPin(P))&=~pinMask(P);digitalHigh(P)
#define pinAsOutput(P) *(ddrOfPin(P))|=pinMask(P)
#define digitalLow(P) *(portOfPin(P))&=~pinMask(P)
#define digitalHigh(P) *(portOfPin(P))|=pinMask(P)
#define isHigh(P)((*(pinOfPin(P))& pinMask(P))>0)
#define isLow(P)((*(pinOfPin(P))& pinMask(P))==0)
#define digitalState(P)((uint8_t)isHigh(P))


/* D3 default */
IRsend irsend;

/*
 * Utils
 */
void _print ( String s ) 	{ Serial.print( s ); }
void _print_end ( void )	{ _print("\r\n"); }

void _err_in ( void )		{ _print("e<"); }
void _err ( String s )		{ _err_in();_print(s);_print_end(); }

void _log_in ( void )		{ _print("l<"); }
void _log ( String s )		{ _log_in();_print(s);_print_end(); }

void _ret_in ( void )		{ _print("r<"); }
void _ret ( String s )		{ _ret_in();_print(s);_print_end(); }

String readline ( void )
{
  String readS = String("");;
  char c = ' ';

  /* Read one command */
  for(;;) {
    if (Serial.available()>0) {
      c = Serial.read(); // Gets one byte from serial buffer
      if (c == '<')
        return readS;
      readS += c;
    }
  }
}

/*
 * Setup
 */
void setup ( )
{
  pinAsOutput( WAIT_LED );
  digitalHigh( WAIT_LED );

  Serial.begin( 57600 );

  /* Wait for serial */
  while (!Serial) { ; }
  delay(100);
  while (Serial.available() == 0) {
    _log("Waiting");
    delay(1000);
  }
  _ret("ok");

  digitalLow( WAIT_LED );
  pinAsInput( RECV_PIN );
}

/*
 * Read code
 */

/* 2 pass - watch the memory ... */
#define READ_PASS_NB		2
#define READ_PAIR_MAX		100

#define READ_RESOLUTION		20
#define WRITE_RESOLUTION	20

#define READ_ADD_RESOLUTION	40
/* This one is critical to detect repeats */
#define READ_GAP_TICKS   	10000/READ_RESOLUTION
#define READ_MAX_TICKS  	65000/READ_RESOLUTION

uint16_t *pulses[READ_PASS_NB];
uint8_t currentpulse[READ_PASS_NB];
uint8_t currentrepeat	= 0;
uint16_t currentgap 	= 0;

void feedCode (uint8_t pass)
{
 uint32_t pulse;      // temporary storage timing
 uint8_t  state = 1;  // need 1 bit: 1 - High, 0 - Low

 currentrepeat	= 0;
 currentgap 	= 0;
 currentpulse[ pass ] = 0;

 for (;;) {
  /* High || Low = state */
  pulse = 0;
  for (;;)
  {
    /* Count number of ticks before state change */
    while (state == (digitalState( RECV_PIN )?1:0)) {
      pulse++;
      delayMicroseconds( READ_RESOLUTION );
      if (currentpulse[pass] != 0)
      {
       if (pulse >= READ_MAX_TICKS)
	return;
      }
    }

    /* New state off and pulse > READ_GAP_TICKS - found a repeat */
    if (currentpulse[pass] != 0 && state && pulse >= READ_GAP_TICKS) {
     currentrepeat ++; /* cnt repeat and move to dry run */
     currentgap = pulse;
    }

    /* End of a High or end of a Low (check if big enough for this one) */
    /*
    if ((! state)
        ||
        pulse >= READ_GAP_TICKS) */
      break;

    pulse = 0;
  }

  if (currentrepeat == 0) {
   pulses[pass][ currentpulse[pass] ] = pulse + (READ_ADD_RESOLUTION/READ_RESOLUTION);
   currentpulse[ pass ] ++;
  }

  if (currentpulse[ pass ] >= READ_PAIR_MAX) {
   _log( "Max pulse event reached - failure" );
   return;
  }

  /* invert state */
  state ^= (uint8_t)1;
 }
}

#define REMAP_PULSE( x ) 	((x * READ_RESOLUTION) / WRITE_RESOLUTION)
void printCode (uint8_t pass)
{
 uint8_t pulse;
 if (currentrepeat>1) {
  _print( String( currentrepeat - 1 ) + '@' );
  _print( String( ( READ_RESOLUTION * currentgap )/1000 ) + '#' );
 }
 _print( String( WRITE_RESOLUTION, DEC ) + "*(" );
 for (pulse=1;  pulse < currentpulse[pass];  pulse++) {
  if (pulse>1)
   _print( "," );
  _print( String( REMAP_PULSE( pulses[pass][pulse] ), DEC ) );
 }
 _print( ")" );
}

#define FREE_PULSE()		for (pass=0; pass < READ_PASS_NB; pass++) \
					  free( pulses[ pass ] )
#define DUMP_PULSE()					\
 for (pass=0; pass < READ_PASS_NB; pass++) { 		\
  _log_in();						\
  _print( "pass "+String(pass)+" " );			\
  printCode( pass );					\
  _print_end();						\
 }

void readCode ()
{
 uint8_t pass, pulse, ref_pulse;

 for (pass=0; pass < READ_PASS_NB; pass++)
 {
  pulses[ pass ] = (uint16_t*) calloc(READ_PAIR_MAX, sizeof(uint16_t));
  _log( "pass "+String(pass+1)+"/"+String(READ_PASS_NB) );
  feedCode( pass );
  delay(200);
 }

 /* currentpulse[X] should match */
 ref_pulse = currentpulse[0];
 for (pass=1; pass < READ_PASS_NB; pass++)
 {
  if (ref_pulse != currentpulse[pass]) {
   DUMP_PULSE();
   _err( "pass error: nb pulse differs" );
   FREE_PULSE();
   return;
  }
 }

 /* put average in [0] */
 for (pulse=0; pulse < ref_pulse; pulse++) {
  int v = 0;
  for (pass=0; pass < READ_PASS_NB; pass++) {
   v += pulses[pass][pulse];
  }
  pulses[0][pulse] = v / READ_PASS_NB;
 }

 _ret_in();
 printCode( 0 );
 _print_end();

 FREE_PULSE();
}

/*
 * Send code
 * Format:
 *      <repeat_nb>@<repeat_sleep_ms>#<resolution>*(<tick_1>,<tick_2>,...)
 *
 * Not mandatory: <repeat_nb>@<repeat_sleep_ms>#
 */
void sendCode (String s)
{
  uint16_t repeat_sleep=0, repeat_nb=0, multi, nb = 0, i;
  int16_t pos_g = 0, pos, s_len;
  unsigned int  irSignal[100];

  /* parse */
  pos = s.indexOf('#');
  if (pos > 0) {
   /* Repeat */
   int16_t pos2 = s.indexOf('@');
   repeat_nb = s.substring(0,pos2).toInt();
   pos2 ++;
   repeat_sleep = s.substring(pos2, pos).toInt();
   pos_g = pos + 1;
   //_log( "Repeat "+String(repeat_nb) );
   //_log( "Repeat sleep "+String(repeat_sleep) );
  }

  pos = s.indexOf('*', pos_g);
  if (pos<0) {
    _err( "wrong format" );
    return;
  }

  multi = s.substring(pos_g, pos).toInt();
  //_log( "Multi " + String(multi) );

  pos_g = pos + 1;
  s_len = s.length();

  if (
    (s.charAt(pos_g) != '(') ||
    (s.charAt(s_len - 1) != ')'))
  {
    _err( "wrong format" );
    return;
  }

  /* Remove parenthesis */
  pos_g ++;
  s_len --;

  /* proc each */
  while (pos_g < s_len)
  {
    pos = s.indexOf(',', pos_g);
    if (pos < 0)
      pos = s_len;

    irSignal[ nb++ ] = multi * s.substring(pos_g, pos).toInt();
    //_log( "Pos: "+ String(irSignal[nb-1]) );

    pos_g = pos + 1;
  }

  /* Send */
  for (i=0; i<=repeat_nb; i++) {
   if (i)
    delay( repeat_sleep );
   irsend.sendRaw(irSignal, nb, 38);
  }
  _ret( "ok" );
}

/*
 * Loop
 */
void loop ( void )
{
  /* Wait for instructions */
  String readS = readline();

  /* Treat the command */
  if (readS.length() > 0) {
    char act;

    act = readS.charAt(0);
    readS = readS.substring(1);

    switch (act)
    {
      case 'r':
        readCode();
        break;
      case 's':
        sendCode(readS);
        break;
      case 'c': /* clear */
        break;
      default:
        _err("Unknown command [ACT:" + String(act) + ", CONTENT:"+readS+"]");
        break;
    }
  }
}

