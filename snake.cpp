
#include <iostream>
#include <string>
#include <vector>
#include <list>
#include <random>
#include <cstdlib>
#include <sys/time.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

/*
 * Header files for X functions
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>

using namespace std;


typedef char DIRECTION;
typedef struct{
	DIRECTION direction;
	int x;
	int y;
	int length;
}shapeBase;

/*
 * Global game state variables
 */
const int Border = 1;
const int BufferSize = 10;
int FPS = 30;
int speed = 5;
const int snakeMPS = 30;
const int giftMPS = 40;
const int width = 800;
const int height = 600;
const int game_y_lo = 50;
const int game_y_hi = 550;
bool fruitDraw = false;
bool scoreBoardDraw = false;
bool insBoardDraw = false;
unsigned long lifePaintTime = 0;
unsigned long lastMoveTime = 0;
unsigned long lifeCountDown = 0;
unsigned long haltBegin = 0;
int life = 1;

enum class GAMEFLAG
{
	gameReady = 1,
	gameRun,
	gameHalt,
	gameOver,
};

GAMEFLAG gameFlag = GAMEFLAG::gameReady;
/*
 * Global CONST direction variables
 */
const DIRECTION WEST 	= 'w';
const DIRECTION EAST 	= 'e';
const DIRECTION NORTH 	= 'n';
const DIRECTION SOUTH 	= 's';

/*
 * Global random engine to reset fruit position
 */

default_random_engine generator;
uniform_int_distribution<int> x_distribution(0,(width/16) - 1); 
uniform_int_distribution<int> y_distribution(0,((game_y_hi-game_y_lo)/16) - 1); 

/*
 * Information to draw on the window.
 */
struct XInfo {
	Display	 *display;
	int		 screen;
	Window	 window;
	GC		 gc[3];
	int		width;		// size of window
	int		height;
};


/*
 * Function to put out a message on error exits.
 */
void error( string str ) {
  cerr << str << endl;
  exit(0);
}


/*
 * An abstract class representing displayable things. 
 */
class Displayable {
	public:
		virtual void paint(XInfo &xinfo) = 0;
		virtual void clearPaint(XInfo &xInfo){
			XSetForeground(xInfo.display, xInfo.gc[0], WhitePixel(xInfo.display, xInfo.screen));
			paint(xInfo);
			XSetForeground(xInfo.display, xInfo.gc[0], BlackPixel(xInfo.display, xInfo.screen));
		};
};       

class Text: public Displayable {
	public:
		virtual void paint(XInfo &xinfo) {
			XFontStruct * font;
			font = XLoadQueryFont(xinfo.display, sFont.c_str());
			XSetFont (xinfo.display, xinfo.gc[0], font->fid);
			XDrawString(xinfo.display, xinfo.window, xinfo.gc[0],
			this->x, this->y, this->sContext.c_str(),this->sContext.length());
		}
		string getText()
		{
			return sContext;
		}

		void setContext(string context)
		{
			sContext = context;
		}

	// constructor
	Text(int x, int y, string s, string font = "12x24")
	: x(x), y(y), sContext(s), sFont(font){}
	private:
	int x;
	int y;
	string sContext;
	string sFont;
};

class Snake : public Displayable {
	public:
		virtual void paint(XInfo &xinfo) {
			drawBody(xinfo);
			drawHead(xinfo);
			drawEye(xinfo);
			drawTail(xinfo);
		}
		
		virtual void clearPaint(XInfo &xInfo){
			XSetForeground(xInfo.display, xInfo.gc[0], WhitePixel(xInfo.display, xInfo.screen));
			//clearTail
			drawTail(xInfo);

			// clear eye
			int headX = getX();
			int headY = getY();
			if (EAST == direction || WEST == direction)
			{
				XDrawArc(xInfo.display, xInfo.window, xInfo.gc[0], headX+radius-eyeRadius, headY-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XDrawArc(xInfo.display, xInfo.window, xInfo.gc[0], headX+radius-eyeRadius, headY+blockSize-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XFillArc(xInfo.display, xInfo.window, xInfo.gc[0], headX+radius-2, headY-2, 4, 4, 0, 360 * 64);
				XFillArc(xInfo.display, xInfo.window, xInfo.gc[0], headX+radius-2, headY+blockSize-2, 4, 4, 0, 360 * 64);	
			}
			else if (SOUTH == direction || NORTH == direction)
			{
				XDrawArc(xInfo.display, xInfo.window, xInfo.gc[0], headX-eyeRadius, headY+radius-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XDrawArc(xInfo.display, xInfo.window, xInfo.gc[0], headX-eyeRadius+blockSize, headY+radius-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XFillArc(xInfo.display, xInfo.window, xInfo.gc[0], headX-2, headY+radius-2, 4, 4, 0, 360 * 64);
				XFillArc(xInfo.display, xInfo.window, xInfo.gc[0], headX+blockSize-2, headY+radius-2, 4, 4, 0, 360 * 64);	
			}
			drawBody(xInfo);
			drawHead(xInfo);
			XSetForeground(xInfo.display, xInfo.gc[0], BlackPixel(xInfo.display, xInfo.screen));
			return;
		};

		// get x for the snake's head
		int getX() {
			if (direction == WEST || direction == NORTH || direction == SOUTH)
			{
				return snakeShape[0].x;
			}
			return snakeShape[0].x+ snakeShape[0].length-blockSize;
		}
		
		// get y for the snake's head
		int getY() {
			if (direction == WEST || direction == EAST || direction == NORTH)
			{
				return snakeShape[0].y;
			}
			return snakeShape[0].y + snakeShape[0].length - blockSize;
		}

		void drawBody(XInfo &xInfo)
		{
			if (snakeShape.size() > 1){
				for (auto it = snakeShape.begin()+1; it != snakeShape.end(); ++it)
				{
					DIRECTION dir = (*it).direction;
					if ((dir == WEST) || (dir == EAST))
					{
						XFillRectangle(xInfo.display, xInfo.window, xInfo.gc[0], (*it).x, (*it).y, (*it).length, blockSize);
					}
					else
					{
						XFillRectangle(xInfo.display, xInfo.window, xInfo.gc[0], (*it).x, (*it).y, blockSize, (*it).length);
					}
				}
			}
		}

		void drawHead(XInfo &xinfo)
		{
			shapeBase headShape = *(snakeShape.begin());
			int nLength = headShape.length;
			int headX = getX();
			int headY = getY();
			if (EAST == direction)
			{
				XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], headShape.x, headShape.y, nLength-radius, blockSize);
			}
			else if (WEST == direction)
			{
				XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], headShape.x+radius, headShape.y, nLength-radius, blockSize);
			}
			else if (SOUTH == direction)
			{
				XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], headShape.x, headShape.y, blockSize, nLength-radius);
			}
			else if (NORTH == direction)
			{
				XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], headShape.x, headShape.y+radius, blockSize, nLength-radius);
			}
			XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX, headY, blockSize, blockSize, 0, 360 * 64);
		}
		
		void drawTail(XInfo &xinfo)
		{
			shapeBase tailShape = *(snakeShape.end()-1);
			int nLength = tailShape.length;
			int tailX = 0;
			int tailY = 0;
			if (EAST == tailShape.direction)
			{
				tailX = tailShape.x - radius;
				tailY = tailShape.y;
			}
			else if (WEST == tailShape.direction)
			{
				tailX = tailShape.x +tailShape.length- radius;
				tailY = tailShape.y;
			}
			else if (SOUTH == tailShape.direction)
			{
				tailX = tailShape.x;
				tailY = tailShape.y-radius;
			}
			else if (NORTH == tailShape.direction)
			{
				tailX = tailShape.x;
				tailY = tailShape.y+tailShape.length-radius;
			}
			XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], tailX, tailY, blockSize, blockSize, 0, 360 * 64);
		}

		void drawEye(XInfo &xinfo)
		{
			int headX = getX();
			int headY = getY();
			XSetForeground(xinfo.display, xinfo.gc[0], WhitePixel(xinfo.display, xinfo.screen));
			if (EAST == direction || WEST == direction)
			{
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+radius-eyeRadius, headY-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+radius-eyeRadius, headY+blockSize-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XSetForeground(xinfo.display, xinfo.gc[0], BlackPixel(xinfo.display, xinfo.screen));
				XDrawArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+radius-eyeRadius, headY-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XDrawArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+radius-eyeRadius, headY+blockSize-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+radius-2, headY-2, 4, 4, 0, 360 * 64);
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+radius-2, headY+blockSize-2, 4, 4, 0, 360 * 64);	
			}
			else if (SOUTH == direction || NORTH == direction)
			{
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX-eyeRadius, headY+radius-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX-eyeRadius+blockSize, headY+radius-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XSetForeground(xinfo.display, xinfo.gc[0], BlackPixel(xinfo.display, xinfo.screen));
				XDrawArc(xinfo.display, xinfo.window, xinfo.gc[0], headX-eyeRadius, headY+radius-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XDrawArc(xinfo.display, xinfo.window, xinfo.gc[0], headX-eyeRadius+blockSize, headY+radius-eyeRadius, eyeRadius*2, eyeRadius*2, 0, 360 * 64);
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX-2, headY+radius-2, 4, 4, 0, 360 * 64);
				XFillArc(xinfo.display, xinfo.window, xinfo.gc[0], headX+blockSize-2, headY+radius-2, 4, 4, 0, 360 * 64);	
			}
		}

		void move(int movePix) {
			if (gameFlag != GAMEFLAG::gameRun)
			{
				return;
			}
			if (EAST == direction || SOUTH == direction)
			{
				snakeShape[0].length = snakeShape[0].length + movePix;
			}
			else if (WEST == direction)
			{
				snakeShape[0].length = snakeShape[0].length + movePix;
				snakeShape[0].x = snakeShape[0].x-movePix;
			}
			else if (NORTH == direction)
			{
				snakeShape[0].length = snakeShape[0].length + movePix;
				snakeShape[0].y = snakeShape[0].y-movePix;
			}
			else
			{
				// an error
			}
			this->updatePaint(movePix);
		}
		
		char getDir(){
			return direction;
		}
		
        /*
         * ** ADD YOUR LOGIC **
         * Use these placeholder methods as guidance for implementing the snake behaviour. 
         * You do not have to use these methods, feel free to implement your own.
         */ 
		
		void increLength(int fruitSize, XInfo &xInfo)
		{
			this->clearPaint(xInfo);
			length += fruitSize;
			if (EAST == direction || SOUTH == direction)
			{
				snakeShape[0].length = snakeShape[0].length + fruitSize;
			}
			else if (WEST == direction)
			{
				snakeShape[0].length = snakeShape[0].length + fruitSize;
				snakeShape[0].x = snakeShape[0].x-fruitSize;
			}
			else if (NORTH == direction)
			{
				snakeShape[0].length = snakeShape[0].length + fruitSize;
				snakeShape[0].y = snakeShape[0].y-fruitSize;
			}
			else
			{
				// an error
			}
			this->paint(xInfo);
		}
		
        bool didEatFruit(const int x, const int y) {
			int snakeX = this->getX();
			int snakeY = this->getY();
			if ((x<= snakeX && snakeX < x+blockSize)
			|| (x < snakeX+blockSize && snakeX+blockSize <= x+blockSize))
			{
				if ((y <= snakeY && snakeY < y+blockSize)
				|| (y < snakeY+blockSize && snakeY+blockSize <= y+blockSize))
				{
					return true;
				}
			}
			return false;
        }

        bool didHitObstacle() {
			bool bHit = false;
			int snakeX = this->getX();
			int snakeY = this->getY();
			// check for border
			if (snakeX < 0 || snakeX+blockSize > width)
			{
				cout << "x error" << endl;
				return true;
			}
			if (snakeY < game_y_lo || snakeY+blockSize > game_y_hi)
			{
				cout << "y error" << endl;
				return true;
			}
			bHit = !this->checkValid(snakeX, snakeY, true);
			if (bHit)
			{
				cout << "hit itself" << endl;
			}
			return bHit;
        }

		void changeDirection(DIRECTION dDir, XInfo &xInfo)
		{
			this->clearPaint(xInfo);
			shapeBase prev = snakeShape[0];
			if (WEST == dDir || EAST == dDir)
			{
				if (NORTH == prev.direction)
				{
					shapeBase newShape{dDir, prev.x, prev.y, blockSize};
					snakeShape.emplace(snakeShape.begin(), newShape);
					direction = dDir;
				}
				else if (SOUTH == prev.direction)
				{
					shapeBase newShape{dDir, prev.x, prev.y+ prev.length-blockSize, blockSize};
					snakeShape.emplace(snakeShape.begin(), newShape);
					direction = dDir;
				}
			}
			if (SOUTH == dDir || NORTH == dDir)
			{
				if (WEST == prev.direction)
				{
					shapeBase newShape{dDir, prev.x, prev.y, blockSize};
					snakeShape.emplace(snakeShape.begin(), newShape);
					direction = dDir;
				}
				else if (EAST == prev.direction)
				{
					shapeBase newShape{dDir, prev.x + prev.length-blockSize, prev.y, blockSize};
					snakeShape.emplace(snakeShape.begin(), newShape);
					direction = dDir;
				}
			}
			this->paint(xInfo);
		}
		
		void updatePaint(int movePix)
		{
			int nDecrease = movePix;
			while(nDecrease > 0)
			{
				auto it = snakeShape.end()-1;
				int myLength = (*it).length-blockSize;
				DIRECTION dDir = (*it).direction;
				if (direction == SOUTH || direction == NORTH)
				{
					int i = 0;
				}
				if (myLength <= nDecrease)
				{
					nDecrease -= myLength;
					snakeShape.pop_back();
				}
				else
				{
					if (dDir == EAST)
					{
						(*it).x = (*it).x+nDecrease;
					}
					if (dDir == SOUTH)
					{
						(*it).y = (*it).y+nDecrease;
					}
					(*it).length = (*it).length-nDecrease;
					nDecrease = 0;
				}
			}
		}
		
		// snake and new fruit are supposed not to share space
		bool checkValid(int x, int y, bool forSnake)
		{
			int bValid = true;
			auto it = snakeShape.begin();
			if (forSnake)
			{
				if (snakeShape.size() <= 2)
				{
					it = snakeShape.end();
				}
				else
				{
					it += 2;
				}
			}
			for (; it != snakeShape.end(); ++it)
			{
				if (it == snakeShape.begin() && forSnake)
				{
					continue;
				}
				DIRECTION dDir = (*it).direction;
				int x_lo = (*it).x;
				int x_hi = (*it).x;
				int y_lo = (*it).y;
				int y_hi = (*it).y;
				if (dDir == WEST || dDir == EAST)
				{
					x_hi = x_lo + (*it).length;
					y_hi = y_lo + blockSize;
				}
				else
				{
					x_hi = x_lo + blockSize;
					y_hi = y_lo + (*it).length;
				}
				if ((x_lo <= x && x < x_hi) 
				|| (x_lo < x+blockSize && x+blockSize <= x_hi))
				{
					if ((y_lo <= y && y < y_hi) 
					|| (y_lo < y+blockSize && y+blockSize <= y_hi))
					{
						bValid = false;
						break;
					}
				}
			}
			return bValid;
		}

		void setSpeed(int nSpeed){
			speed = nSpeed;
		}

		int getSpeed(){
			return speed;
		}

		void reset()
		{
			speed = 5;
            blockSize = 16;
			radius = blockSize/2;
			eyeRadius = 5;
			direction = EAST;
			length = 45;
			snakeShape.clear();
			shapeBase shape {EAST,x,y,length};
			snakeShape.push_back(shape);
		}
		Snake(int x, int y): x(x), y(y) {
			speed = 5;
            blockSize = 16;
			radius = blockSize/2;
			eyeRadius = 5;
			direction = EAST;
			length = 45;
			shapeBase shape {EAST,x,y,length};
			snakeShape.push_back(shape);
		}
	
	private:
		int x;
		int y;
		int blockSize;
		int radius;
		int eyeRadius;
		DIRECTION direction;
		int speed;
		int length;
		vector<shapeBase> snakeShape;
};

class Fruit : public Displayable {
	public:
		virtual void paint(XInfo &xInfo) {
			XDrawArc(xInfo.display, xInfo.window, xInfo.gc[0], x, y, 16, 16, 0, 360 * 64);
			XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],x+4, y+5, x+12, y+5);
			XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],x+8, y+5, x+8, y-3);
        }

        Fruit() {
            x = 50;
            y = 100;
        }
		
		void reset()
		{
			x = 50;
			y = 100;
		}

		int getX()
		{
			return x;
		}

		int getY()
		{
			return y;
		}

		void newPos(Snake &snake)
		{
			int new_x = x_distribution(generator) * 16;
			int new_y = game_y_lo +5+ y_distribution(generator) * 16;
			while(true)
			{
				if ((new_x != x || new_y != y) && snake.checkValid(new_x, new_y, false))
				{
					x = new_x;
					y = new_y;
					break;
				}
				new_x = x_distribution(generator) * 16;	
				new_y = game_y_lo +5+ y_distribution(generator) * 16;
			}
		}

    private:
        int x;
        int y;
};

class Life	: public Displayable{
	public:
		virtual void paint(XInfo &xInfo) {
			if (bON)
			{
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX, lifeY+8, lifeX+8, lifeY+16);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+16, lifeY+8, lifeX+8, lifeY+16);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX, lifeY+8, lifeX, lifeY+6);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+16, lifeY+8, lifeX+16, lifeY+6);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX, lifeY+6, lifeX+2, lifeY);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+16, lifeY+6, lifeX+14, lifeY);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+2, lifeY, lifeX+6, lifeY);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+10, lifeY, lifeX+14, lifeY);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+6, lifeY, lifeX+8, lifeY+6);
				XDrawLine(xInfo.display, xInfo.window, xInfo.gc[0],lifeX+10, lifeY, lifeX+8, lifeY+6);
			}
		}

		virtual void clearPaint(XInfo &xInfo)
		{
			if (bON)
			{
				XSetForeground(xInfo.display, xInfo.gc[0], WhitePixel(xInfo.display, xInfo.screen));
				this->paint(xInfo);
				XSetForeground(xInfo.display, xInfo.gc[0], BlackPixel(xInfo.display, xInfo.screen));
			}
		}

		void move(int movePix){
			if (bON){
				lifeY += movePix;
			}
		}

		bool reachBottom()
		{
			return (lifeY + 16) >= game_y_hi;
		}

		bool bEaten(Snake &snake, XInfo &xInfo)
		{
			if (bON){
				if (snake.checkValid(lifeX, lifeY, false)){
					return false;
				}
				else
				{
					bON = true;
					this->clearPaint(xInfo);
					this->reset();
				}
				return true;
			}
			return false;
		}

		bool isOn()
		{
			return bON;
		}

		void reset()
		{
			lifeX = x_distribution(generator)*16;
			lifeY = game_y_lo;
			bON = false;
		}

		void turnOn()
		{
			bON = true;
		}

		Life(){
			lifeX = x_distribution(generator)*16;
			lifeY = game_y_lo;
			bON = false;
		}
		private:
		int lifeX;
		int lifeY;
		bool bON;
};

class InsBoard : public Displayable {
	public:
		virtual void paint(XInfo &xinfo) {
			XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], 0, game_y_hi, width, lineHeight);
			for (auto it = InsBoardInfo.begin(); it != InsBoardInfo.end(); ++it)
			{
				it->paint(xinfo);
			}
			insBoardDraw = true;
		}

		void drawLine(XInfo &xinfo){
			XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], 0, game_y_hi, width, lineHeight);
		}

		InsBoard(){
			// index 0 for pauseInfo
			Text pauseInfo{30, 580, "p-pause", "*x15"};
			InsBoardInfo.push_back(pauseInfo);
			// index 1 for resumeInfo
			Text resumeInfo{135, 580, "o-resume", "*x15"};
			InsBoardInfo.push_back(resumeInfo);
			// index 2 for quitInfo
			Text quitInfo{240, 580, "q-quit", "*x15"};
			InsBoardInfo.push_back(quitInfo);
			// index 3 for restartInfo
			Text restartInfo{330, 580, "r-restart", "*x15"};
			InsBoardInfo.push_back(restartInfo);
			// index 4 for restartInfo
			Text operationInfo{450, 580, "w/a/s/d/arrow keys-control the snake", "*x15"};
			InsBoardInfo.push_back(operationInfo);
		}
	private:
	int lineHeight = 5;
	vector<Text> InsBoardInfo;
};

class ScoreBoard : public Displayable {
	public:
		virtual void paint(XInfo &xinfo) {
			XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], 0, game_y_lo-lineHeight, width, lineHeight);
			for (auto it = ScoreBoardInfo.begin(); it != ScoreBoardInfo.end(); ++it)
			{
				it->paint(xinfo);
			}
			scoreBoardDraw = true;
		}

		void drawLine(XInfo &xinfo){
			XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], 0, game_y_lo-lineHeight, width, lineHeight);
		}

		void updateScore(XInfo &xinfo)
		{
			// clear old score
			ScoreBoardInfo[3].clearPaint(xinfo);
			// calculate and draw new score
			int nScore = stoi(ScoreBoardInfo[3].getText());
			nScore++;
			string sScore = to_string(nScore);
			ScoreBoardInfo[3].setContext(sScore);
			ScoreBoardInfo[3].paint(xinfo);
		}

		void updateLife(XInfo &xinfo)
		{
			// clear old life
			ScoreBoardInfo[7].clearPaint(xinfo);
			// calculate and draw new life
			string sLife = to_string(life);
			ScoreBoardInfo[7].setContext(sLife);
			ScoreBoardInfo[7].paint(xinfo);
		}

		void updateSetting()
		{
			ScoreBoardInfo[4].setContext(to_string(FPS));
			ScoreBoardInfo[5].setContext(to_string(speed));
		}

		void reset()
		{
			ScoreBoardInfo[3].setContext(to_string(0));
			ScoreBoardInfo[7].setContext(to_string(1));
		}

		ScoreBoard(){
			// index 0 for scoreText
			Text scoreText{30, 30, "Score: "};
			ScoreBoardInfo.push_back(scoreText);
			// index 1 for frameText
			Text frameText{500, 30, "Frame: "};
			ScoreBoardInfo.push_back(frameText);
			// index 2 for speedText
			Text speedText{650, 30, "Speed: "};
			ScoreBoardInfo.push_back(speedText);
			// index 3 for scoreValue
			Text scoreValue{100, 30, "0"};
			ScoreBoardInfo.push_back(scoreValue);
			// index 4 for frameValue
			Text frameValue{570, 30, to_string(FPS)};
			ScoreBoardInfo.push_back(frameValue);
			// index 5 for speedValue
			Text speedValue{720, 30, to_string(speed)};
			ScoreBoardInfo.push_back(speedValue);
			// index 6 for LifeText
			Text lifeText{150, 30, "Life: "};
			ScoreBoardInfo.push_back(lifeText);
			// index 7 for LifeValue
			Text lifeValue{220, 30, "1"};
			ScoreBoardInfo.push_back(lifeValue);
		}
	private:
	string sScore = "Score:";
	int lineHeight = 5;
	vector<Text> ScoreBoardInfo;
};

class Instruction : public Displayable{
	public:
		virtual void paint(XInfo &xinfo)
		{
			for (auto it = insInfo.begin(); it != insInfo.end(); ++it)
			{
				it ->paint(xinfo);
			}
		}

		Instruction()
		{
			Text start{300, 200, "Press 'r' to start game", "*x15"};
			Text quit{300, 230, "Press 'q' to quit game", "*x15"};
			Text halt{300, 260, "Press 'p' to pause game", "*x15"};
			Text resume{300, 290, "Press 'o' to resume game", "*x15"};
			Text control_1{300, 320, "Press 'w/a/s/d' to control the snake", "*x15"};
			Text control_2{300, 350, "Press arrow keys to control the snake", "*x15"};
			insInfo.push_back(start);
			insInfo.push_back(quit);
			insInfo.push_back(halt);
			insInfo.push_back(resume);
			insInfo.push_back(control_1);
			insInfo.push_back(control_2);
		}
		vector<Text> insInfo;
};

list<Displayable *> dList;           // list of Displayables
Snake snake(100, 450);
Fruit fruit;
ScoreBoard scoreboard;
InsBoard insboard;
Life lifeGift;

// information when game over
Text gameOverText(360, 300, "GAME OVER", "*x24");

// information when game start
Text gameTitle(360, 100, "Snake", "*x24");
Instruction instruction;

/*
 * Initialize X and create a window
 */
void initX(int argc, char *argv[], XInfo &xInfo) {
	XSizeHints hints;
	unsigned long white, black;

   /*
	* Display opening uses the DISPLAY	environment variable.
	* It can go wrong if DISPLAY isn't set, or you don't have permission.
	*/	
	xInfo.display = XOpenDisplay( "" );
	if ( !xInfo.display )	{
		error( "Can't open display." );
	}
	
   /*
	* Find out some things about the display you're using.
	*/
	xInfo.screen = DefaultScreen( xInfo.display );

	white = XWhitePixel( xInfo.display, xInfo.screen );
	black = XBlackPixel( xInfo.display, xInfo.screen );

	hints.x = 100;
	hints.y = 100;
	hints.width = 800;
	hints.height = 600;
	hints.flags = PPosition | PSize;

	xInfo.window = XCreateSimpleWindow( 
		xInfo.display,				// display where window appears
		DefaultRootWindow( xInfo.display ), // window's parent in window tree
		hints.x, hints.y,			// upper left corner location
		hints.width, hints.height,	// size of the window
		Border,						// width of window's border
		black,						// window border colour
		white );					// window background colour
		
	XSetStandardProperties(
		xInfo.display,		// display containing the window
		xInfo.window,		// window whose properties are set
		"animation",		// window's title
		"Animate",			// icon's title
		None,				// pixmap for the icon
		argv, argc,			// applications command line args
		&hints );			// size hints for the window

	/* 
	 * Create Graphics Contexts
	 */
	int i = 0;
	xInfo.gc[i] = XCreateGC(xInfo.display, xInfo.window, 0, 0);
	XSetForeground(xInfo.display, xInfo.gc[i], BlackPixel(xInfo.display, xInfo.screen));
	XSetBackground(xInfo.display, xInfo.gc[i], WhitePixel(xInfo.display, xInfo.screen));
	XSetFillStyle(xInfo.display, xInfo.gc[i], FillSolid);
	XSetLineAttributes(xInfo.display, xInfo.gc[i],
	                     1, LineSolid, CapButt, JoinRound);

	XSelectInput(xInfo.display, xInfo.window, 
		ButtonPressMask | KeyPressMask | 
		PointerMotionMask | 
		EnterWindowMask | LeaveWindowMask |
		StructureNotifyMask);  // for resize events
	/*
	 * Put the window on the screen.
	 */
	XMapRaised( xInfo.display, xInfo.window );
	XFlush(xInfo.display);
}

/*
 * Function to repaint a display list
 */
void repaint( XInfo &xinfo) {
	list<Displayable *>::const_iterator begin = dList.begin();
	list<Displayable *>::const_iterator end = dList.end();

	XClearWindow( xinfo.display, xinfo.window );
	
	// get height and width of window (might have changed since last repaint)

	XWindowAttributes windowInfo;
	XGetWindowAttributes(xinfo.display, xinfo.window, &windowInfo);
	unsigned int height = windowInfo.height;
	unsigned int width = windowInfo.width;

	// big black rectangle to clear background
    
	// draw display list
	while( begin != end ) {
		Displayable *d = *begin;
		d->paint(xinfo);
		begin++;
	}
	XFlush( xinfo.display );
}

// get microseconds
unsigned long now() {
	timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000000 + tv.tv_usec;
}

void gameOver(XInfo &xinfo)
{
	XSetForeground(xinfo.display, xinfo.gc[0], WhitePixel(xinfo.display, xinfo.screen));
	XFillRectangle(xinfo.display, xinfo.window, xinfo.gc[0], 0, game_y_lo, width, 500);
	XSetForeground(xinfo.display, xinfo.gc[0], BlackPixel(xinfo.display, xinfo.screen));
	gameOverText.paint(xinfo);
	gameFlag = GAMEFLAG::gameReady;
}

void handleAnimation(XInfo &xinfo, int inside) {
	unsigned long currentTime = now();
	
	scoreboard.drawLine(xinfo);
	insboard.drawLine(xinfo);

	// life gift appears every 10 second
	if (currentTime-lifeCountDown >= (10*1000000) && !lifeGift.isOn())
	{
		lifeGift.turnOn();
		lifePaintTime = currentTime;
	}

	// handle for snake paint
	snake.clearPaint(xinfo);
	int movePix = (snakeMPS*speed)*(currentTime-lastMoveTime)/1000000;
	snake.move(movePix);
	if (movePix > 0)
	{
		// after each movement update time
		lastMoveTime = currentTime;
	}
	snake.paint(xinfo);

	// handle for lifeGift paint
	lifeGift.clearPaint(xinfo);
	int giftMovePix = (giftMPS*speed)*(currentTime-lifePaintTime)/1000000;
	lifeGift.move(giftMovePix);
	if (giftMovePix > 0)
	{
		// after each movement update time
		lifePaintTime = currentTime;

	}
	lifeGift.paint(xinfo);



	if (lifeGift.bEaten(snake, xinfo))
	{
		lifeGift.clearPaint(xinfo);
		lifeGift.reset();
		lifeCountDown = currentTime;
		life++;
		scoreboard.updateLife(xinfo);
	}
	if (lifeGift.reachBottom())
	{
		lifeGift.clearPaint(xinfo);
		lifeGift.reset();
		lifeCountDown = currentTime;
	}

	if (snake.didHitObstacle())
	{
		life--;
		scoreboard.updateLife(xinfo);
		if(0 >= life){
			gameFlag = GAMEFLAG::gameOver;
			gameOver(xinfo);
			return;
		}
		else
		{
			snake.clearPaint(xinfo);
			snake.reset();
		}
	}
	if (snake.didEatFruit(fruit.getX(), fruit.getY()))
	{
		fruit.clearPaint(xinfo);
		snake.increLength(16, xinfo);
		fruit.newPos(snake);
		fruitDraw = false;
		scoreboard.updateScore(xinfo);
	}
	if (!fruitDraw)
	{
		fruit.paint(xinfo);
	}
}

void handleForGameRun(XInfo &xinfo, char cKey, int inside)
{
	if (cKey == 'w' || cKey == 'W') {
		snake.changeDirection(NORTH, xinfo);
	}
	if (cKey == 'a' || cKey == 'A') {
		snake.changeDirection(WEST, xinfo);
	}
	if (cKey == 's' || cKey == 'S') {
		snake.changeDirection(SOUTH, xinfo);
	}
	if (cKey == 'd' || cKey == 'D') {
		snake.changeDirection(EAST, xinfo);
	}
	if (cKey == 'p' || cKey == 'P') {
		gameFlag = GAMEFLAG::gameHalt;
		haltBegin = now();
		return;
	}
	handleAnimation(xinfo, inside);
}

void handleForGameHalt(XInfo &xinfo, char cKey, int inside)
{
	if (cKey == 'o' || cKey == 'O')
	{
		gameFlag = GAMEFLAG::gameRun;
		unsigned long haltTimeInterval = now()-haltBegin;
		lastMoveTime += haltTimeInterval;
		lifeCountDown += haltTimeInterval;
		lifePaintTime += haltTimeInterval;
	}
}

void handleForGameReady(XInfo &xinfo, char cKey, int inside)
{
	if (cKey == 'r' || cKey == 'R')
	{
		snake.reset();
		fruit.reset();
		scoreboard.reset();
		fruitDraw = false;
		repaint(xinfo);
		
		life = 1;
		lifeCountDown = 0;
		lifePaintTime = 0;
		lastMoveTime = 0;
		gameFlag = GAMEFLAG::gameRun;
	}
}

char handleKeyPress(XInfo &xinfo, XEvent &event) {
	KeySym key;
	char text[BufferSize];
	XKeyPressedEvent * pressedKey = (XKeyPressedEvent *)&event;

	/*
	 * Exit when 'q' is typed.
	 * This is a simplified approach that does NOT use localization.
	 */
	int i = XLookupString( 
		(XKeyEvent *)&event, 	// the keyboard event
		text, 					// buffer when text will be written
		BufferSize, 			// size of the text buffer
		&key, 					// workstation-independent key symbol
		NULL );					// pointer to a composeStatus structure (unused)
	if ( i == 1) {
		printf("Got key press -- %c\n", text[0]);
		if (text[0] == 'q' || text[0] == 'Q') {
			error("Terminating normally.");
		}
	}
	char cKey = text[0];
	switch(key){
		case XK_Up:
			cout << "Up" << endl;
			cKey = 'w';
			break;
		case XK_Down:
			cout << "Down" << endl;
			return 's';
			break;
		case XK_Left:
			cout << "Left" << endl;
			cKey = 'a';
			break;
		case XK_Right:
			cout << "Right" << endl;
			cKey = 'd';
			break;
	}
	return cKey;
}

void printStartInfo(XInfo xinfo)
{
	gameTitle.paint(xinfo);
	instruction.paint(xinfo);
}

void eventLoop(XInfo &xinfo) {
	// Add stuff to paint to the display list
	dList.push_front(&snake);
    dList.push_front(&fruit);
	dList.push_front(&scoreboard);
	dList.push_front(&insboard);
	XEvent event;
	unsigned long lastRepaint = 0;
	int inside = 0;
	char cKey;
	
	// wait for XWindow to set up
	usleep(100000);
	printStartInfo(xinfo);
	
	while( true ) {
		if (XPending(xinfo.display) > 0) {
			XNextEvent( xinfo.display, &event );
			cout << "event.type=" << event.type << "\n";
			switch( event.type ) {
				case KeyPress:
					cKey = handleKeyPress(xinfo, event);
					break;
				case EnterNotify:
					inside = 1;
					break;
				case LeaveNotify:
					inside = 0;
					break;
			}
		}
		unsigned long end = now();	// get time in microsecond
		if (end - lastRepaint > 1000000 /FPS){
			switch(gameFlag)
			{
				case GAMEFLAG::gameRun	:{
					if (0 == lifeCountDown){
						// init lifeCountDown
						lifeCountDown = now();}
					if (0 == lifePaintTime){
						// init lifePaintTime
						lifePaintTime = now();}
					if (0 == lastMoveTime){
						// init sanke moveTime
						lastMoveTime = now();}
					handleForGameRun(xinfo, cKey, inside);
					break;
				}
				case GAMEFLAG::gameHalt	:{
					handleForGameHalt(xinfo, cKey, inside);
					break;
				}
				case GAMEFLAG::gameReady:{
					handleForGameReady(xinfo, cKey, inside);
					break;
				}
			}
			lastRepaint = now();
		}
		if (XPending(xinfo.display) == 0) {
			// at most sleep for 1000000/FPS msec
			unsigned long timeInterval = 0;
			if ((end - lastRepaint) > 0)
			{
				timeInterval = end-lastRepaint;
			}
			
			usleep(1000000 / FPS - timeInterval);
		}
	}
}


/*
 * Start executing here.
 *	 First initialize window.
 *	 Next loop responding to events.
 *	 Exit forcing window manager to clean up - cheesy, but easy.
 */
int main ( int argc, char *argv[] ) {
	if (argc == 3)
	{
		int nFrame = atoi(argv[1]);
		int nSpeed = atoi(argv[2]);
		if (1 <= nFrame && nFrame <= 100)
		{
			FPS = nFrame;
		}
		if (1 <= nSpeed && nSpeed <= 10)
		{
			speed = nSpeed;
		}
		scoreboard.updateSetting();
	}
	XInfo xInfo;
	initX(argc, argv, xInfo);
	
	eventLoop(xInfo);
	XCloseDisplay(xInfo.display);
}
