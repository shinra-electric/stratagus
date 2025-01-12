//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name widgets.h - The widgets headerfile. */
//
//      (c) Copyright 2005-2006 by Fran�ois Beerten and Jimmy Salmon
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

#ifndef __WIDGETS_H__
#define __WIDGETS_H__

#include <functional>

#include <guichan.h>
#include <guichan/gsdl.h>
#include "font.h"

#include "keylistener.h"
#include "luacallback.h"
#include "mouselistener.h"


extern bool GuichanActive;

void initGuichan();
void freeGuichan();
void handleInput(const SDL_Event *event);

class LuaActionListener : public gcn::ActionListener, public gcn::KeyListener, public gcn::MouseListener
{
	LuaCallback callback;
public:
	LuaActionListener(lua_State *lua, lua_Object function);
	virtual void action(const std::string &eventId);
	virtual bool keyPress(const gcn::Key&);
	virtual bool keyRelease(const gcn::Key&);
	virtual void hotKeyPress(const gcn::Key&);
	virtual void hotKeyRelease(const gcn::Key&);
	virtual void mouseIn();
	virtual void mouseOut();
	virtual void mousePress(int, int, int);
	virtual void mouseRelease(int, int, int);
	virtual void mouseClick(int, int, int, int);
	virtual void mouseWheelUp(int, int);
	virtual void mouseWheelDown(int, int);
	virtual void mouseMotion(int, int);
	virtual ~LuaActionListener();
};

class LambdaActionListener : public gcn::ActionListener
{
	std::function<void(const std::string &)> lambda;
public:
	LambdaActionListener(std::function<void(const std::string &)> l)
	{
		lambda = l;
	}

	virtual void action(const std::string &eventId)
	{
		lambda(eventId);
	}
};

class ImageWidget : public gcn::Icon
{
public:
	explicit ImageWidget(gcn::Image *img) : gcn::Icon(img) {}
};

class ButtonWidget : public gcn::Button
{
public:
	explicit ButtonWidget(const std::string &caption) : Button(caption)
	{
		this->setHotKey(GetHotKey(caption));
	}
};

class ImageButton : public gcn::Button
{
public:
	ImageButton();
	explicit ImageButton(const std::string &caption);

	virtual void draw(gcn::Graphics *graphics);
	virtual void adjustSize();

	void setNormalImage(gcn::Image *image) { normalImage = image; adjustSize(); }
	void setPressedImage(gcn::Image *image) { pressedImage = image; }
	void setDisabledImage(gcn::Image *image) { disabledImage = image; }

	gcn::Image *normalImage;
	gcn::Image *pressedImage;
	gcn::Image *disabledImage;
};

class ImageRadioButton : public gcn::RadioButton
{
public:
	ImageRadioButton();
	ImageRadioButton(const std::string &caption, const std::string &group,
					 bool marked);

	virtual void drawBox(gcn::Graphics *graphics);
	virtual void draw(gcn::Graphics *graphics);

	virtual void mousePress(int x, int y, int button);
	virtual void mouseRelease(int x, int y, int button);
	virtual void mouseClick(int x, int y, int button, int count);
	virtual void adjustSize();

	void setUncheckedNormalImage(gcn::Image *image) { uncheckedNormalImage = image; }
	void setUncheckedPressedImage(gcn::Image *image) { uncheckedPressedImage = image; }
	void setUncheckedDisabledImage(gcn::Image *image) { uncheckedDisabledImage = image; }
	void setCheckedNormalImage(gcn::Image *image) { checkedNormalImage = image; }
	void setCheckedPressedImage(gcn::Image *image) { checkedPressedImage = image; }
	void setCheckedDisabledImage(gcn::Image *image) { checkedDisabledImage = image; }

	gcn::Image *uncheckedNormalImage;
	gcn::Image *uncheckedPressedImage;
	gcn::Image *uncheckedDisabledImage;
	gcn::Image *checkedNormalImage;
	gcn::Image *checkedPressedImage;
	gcn::Image *checkedDisabledImage;
	bool mMouseDown;
};

class ImageCheckBox : public gcn::CheckBox
{
public:
	ImageCheckBox();
	ImageCheckBox(const std::string &caption, bool marked = false);

	virtual void draw(gcn::Graphics *graphics);
	virtual void drawBox(gcn::Graphics *graphics);

	virtual void mousePress(int x, int y, int button);
	virtual void mouseRelease(int x, int y, int button);
	virtual void mouseClick(int x, int y, int button, int count);
	virtual void adjustSize();

	void setUncheckedNormalImage(gcn::Image *image) { uncheckedNormalImage = image; }
	void setUncheckedPressedImage(gcn::Image *image) { uncheckedPressedImage = image; }
	void setUncheckedDisabledImage(gcn::Image *image) { uncheckedDisabledImage = image; }
	void setCheckedNormalImage(gcn::Image *image) { checkedNormalImage = image; }
	void setCheckedPressedImage(gcn::Image *image) { checkedPressedImage = image; }
	void setCheckedDisabledImage(gcn::Image *image) { checkedDisabledImage = image; }

	gcn::Image *uncheckedNormalImage;
	gcn::Image *uncheckedPressedImage;
	gcn::Image *uncheckedDisabledImage;
	gcn::Image *checkedNormalImage;
	gcn::Image *checkedPressedImage;
	gcn::Image *checkedDisabledImage;
	bool mMouseDown;
};

class ImageSlider : public gcn::Slider
{
public:
	explicit ImageSlider(double scaleEnd = 1.0);
	ImageSlider(double scaleStart, double scaleEnd);

	virtual void drawMarker(gcn::Graphics *graphics);
	virtual void draw(gcn::Graphics *graphics);

	void setMarkerImage(gcn::Image *image);
	void setBackgroundImage(gcn::Image *image);
	void setDisabledBackgroundImage(gcn::Image *image);

	gcn::Image *markerImage;
	gcn::Image *backgroundImage;
	gcn::Image *disabledBackgroundImage;
};

class MultiLineLabel : public gcn::Widget
{
public:
	MultiLineLabel();
	explicit MultiLineLabel(const std::string &caption);

	virtual void setCaption(const std::string &caption);
	virtual const std::string &getCaption() const;
	virtual void setAlignment(unsigned int alignment);
	virtual unsigned int getAlignment();
	virtual void setVerticalAlignment(unsigned int alignment);
	virtual unsigned int getVerticalAlignment();
	virtual void setLineWidth(int width);
	virtual int getLineWidth();
	virtual void adjustSize();
	virtual void draw(gcn::Graphics *graphics);
	virtual void drawBorder(gcn::Graphics *graphics);

	enum {
		LEFT = 0,
		CENTER,
		RIGHT,
		TOP,
		BOTTOM
	};

private:
	void wordWrap();

	std::string mCaption;
	std::vector<std::string> mTextRows;
	unsigned int mAlignment;
	unsigned int mVerticalAlignment;
	int mLineWidth;
};

class ScrollingWidget : public gcn::ScrollArea
{
public:
	ScrollingWidget(int width, int height);
	void add(gcn::Widget *widget, int x, int y);
	void restart();
	void setSpeed(float speed) { this->speedY = speed; }
	float getSpeed() const { return this->speedY; }
private:
	virtual void logic();
private:
	gcn::Container container; /// Data container
	float speedY;             /// vertical speed of the container (positive number: go up).
	float containerY;         /// Y position of the container
	bool finished;            /// True while scrolling ends.
};

class Windows : public gcn::Window
{
public:
	Windows(const std::string &text, int width, int height);
	void add(gcn::Widget *widget, int x, int y);
private:
	virtual void mouseMotion(int x, int y);
	virtual void setBackgroundColor(const gcn::Color &color);
	virtual void setBaseColor(const gcn::Color &color);
private:
	gcn::ScrollArea scroll;   /// To use scroll bar.
	gcn::Container container; /// data container.
	bool blockwholewindow;    /// Manage condition limit of moveable windows. @see mouseMotion.
	/// @todo Method to set this variable. Maybe set the variable static.
};

class ImageTextField : public gcn::TextField
{

public:
	ImageTextField() : TextField(), itemImage(nullptr) {}
	ImageTextField(const std::string& text) : gcn::TextField(text), itemImage(nullptr) {}
	virtual void draw(gcn::Graphics *graphics);
	virtual void drawBorder(gcn::Graphics *graphics);
	void setItemImage(CGraphic *image) { itemImage = image; }
private:
	CGraphic *itemImage;
};

class StringListModel : public gcn::ListModel
{
	std::vector<std::string> list;
public:
	StringListModel(std::vector<std::string> l) {
		list = l;
	}

	virtual int getNumberOfElements() { return list.size(); }
	virtual std::string getElementAt(int i) { return list[i]; }
};

class LuaListModel : public gcn::ListModel
{
	std::vector<std::string> list;
public:
	LuaListModel() {}

	void setList(lua_State *lua, lua_Object *lo);
	virtual int getNumberOfElements() {return list.size();}
	virtual std::string getElementAt(int i) {return list[i];}
};

class ImageListBox : public gcn::ListBox
{
public:
	ImageListBox();
	ImageListBox(gcn::ListModel *listModel);
	virtual void draw(gcn::Graphics *graphics);
	virtual void drawBorder(gcn::Graphics *graphics);
	void setItemImage(CGraphic *image) { itemImage = image; }
	void adjustSize();
	void mousePress(int, int y, int button);
	void setSelected(int selected);
	void setListModel(gcn::ListModel *listModel);
	void logic()
	{
		adjustSize();
	}
private:
	CGraphic *itemImage;
};

class ListBoxWidget : public gcn::ScrollArea
{
public:
	ListBoxWidget(unsigned int width, unsigned int height);
	void setList(lua_State *lua, lua_Object *lo);
	void setSelected(int i);
	int getSelected() const;
	virtual void setBackgroundColor(const gcn::Color &color);
	virtual void setFont(gcn::Font *font);
	virtual void addActionListener(gcn::ActionListener *actionListener);
private:
	void adjustSize();
private:
	LuaListModel lualistmodel;
	gcn::ListBox listbox;
};

class ImageListBoxWidget : public ListBoxWidget
{
public:
	ImageListBoxWidget(unsigned int width, unsigned int height);
	void setList(lua_State *lua, lua_Object *lo);
	void setSelected(int i);
	int getSelected() const;
	virtual void setBackgroundColor(const gcn::Color &color);
	virtual void setFont(gcn::Font *font);
	virtual void addActionListener(gcn::ActionListener *actionListener);

	void setItemImage(CGraphic *image) {
		itemImage = image;
		listbox.setItemImage(image);
	}
	void setUpButtonImage(CGraphic *image) { upButtonImage = image; }
	void setUpPressedButtonImage(CGraphic *image) { upPressedButtonImage = image; }
	void setDownButtonImage(CGraphic *image) { downButtonImage = image; }
	void setDownPressedButtonImage(CGraphic *image) { downPressedButtonImage = image; }
	void setLeftButtonImage(CGraphic *image) { leftButtonImage = image; }
	void setLeftPressedButtonImage(CGraphic *image) { leftPressedButtonImage = image; }
	void setRightButtonImage(CGraphic *image) { rightButtonImage = image; }
	void setRightPressedButtonImage(CGraphic *image) { rightPressedButtonImage = image; }
	void setHBarImage(CGraphic *image) {
		hBarButtonImage = image;
		mScrollbarWidth = std::min<int>(image->getWidth(), image->getHeight());
	}
	void setVBarImage(CGraphic *image) {
		vBarButtonImage = image;
		mScrollbarWidth = std::min<int>(image->getWidth(), image->getHeight());
	}
	void setMarkerImage(CGraphic *image) { markerImage = image; }

	virtual void draw(gcn::Graphics *graphics);
	virtual void drawBorder(gcn::Graphics *graphics);
	virtual gcn::Rectangle getVerticalMarkerDimension();
	virtual gcn::Rectangle getHorizontalMarkerDimension();
private:
	void adjustSize();

	void drawUpButton(gcn::Graphics *graphics);
	void drawDownButton(gcn::Graphics *graphics);
	void drawLeftButton(gcn::Graphics *graphics);
	void drawRightButton(gcn::Graphics *graphics);
	void drawUpPressedButton(gcn::Graphics *graphics);
	void drawDownPressedButton(gcn::Graphics *graphics);
	void drawLeftPressedButton(gcn::Graphics *graphics);
	void drawRightPressedButton(gcn::Graphics *graphics);
	void drawHMarker(gcn::Graphics *graphics);
	void drawVMarker(gcn::Graphics *graphics);
	void drawHBar(gcn::Graphics *graphics);
	void drawVBar(gcn::Graphics *graphics);
private:
	CGraphic *itemImage;
	CGraphic *upButtonImage;
	CGraphic *upPressedButtonImage;
	CGraphic *downButtonImage;
	CGraphic *downPressedButtonImage;
	CGraphic *leftButtonImage;
	CGraphic *leftPressedButtonImage;
	CGraphic *rightButtonImage;
	CGraphic *rightPressedButtonImage;
	CGraphic *hBarButtonImage;
	CGraphic *vBarButtonImage;
	CGraphic *markerImage;

	LuaListModel lualistmodel;
	ImageListBox listbox;
};

class DropDownWidget : public gcn::DropDown
{
	LuaListModel listmodel;
public:
	DropDownWidget() {}
	void setList(lua_State *lua, lua_Object *lo);
	virtual void setSize(int width, int height);
};

class ImageDropDownWidget : public DropDownWidget
{
public:
	ImageDropDownWidget() : itemImage(nullptr) {
		mListBox.addActionListener(this);
		setListModel(&listmodel);
		mScrollArea->setContent(&mListBox);
	}
	void setItemImage(CGraphic *image) {
		itemImage = image;
		mListBox.setItemImage(image);
	}
	void setDownNormalImage(CGraphic *image) { DownNormalImage = image; }
	void setDownPressedImage(CGraphic *image) { DownPressedImage = image; }

	virtual ImageListBox *getListBox() { return &mListBox; }
	virtual void draw(gcn::Graphics *graphics);
	virtual void drawBorder(gcn::Graphics *graphics);
	void drawButton(gcn::Graphics *graphics);
	void setList(lua_State *lua, lua_Object *lo);
	virtual void setSize(int width, int height);
	void setListModel(LuaListModel *listModel);
	int getSelected();
	void setSelected(int selected);
	void adjustHeight();
	void setListBox(ImageListBox *listBox);
	void setFont(gcn::Font *font);
	void _mouseInputMessage(const gcn::MouseInput &mouseInput);
private:
	CGraphic *itemImage;
	CGraphic *DownNormalImage;
	CGraphic *DownPressedImage;
	ImageListBox mListBox;
	LuaListModel listmodel;
};

class StatBoxWidget : public gcn::Widget
{
public:
	StatBoxWidget(int width, int height);

	virtual void draw(gcn::Graphics *graphics);
	void setCaption(const std::string &s);
	const std::string &getCaption() const;
	void setPercent(const int percent);
	int getPercent() const;

private:
	std::string caption;  /// caption of the widget.
	unsigned int percent; /// percent value of the widget.
};

class MenuScreen : public gcn::Container
{
public:
	MenuScreen();

	int run(bool loop = true);
	void stop(int result = 0, bool stopAll = false);
	void stopAll(int result = 0) { stop(result, true); }
	void addLogicCallback(LuaActionListener *listener);
	virtual void draw(gcn::Graphics *graphics);
	virtual void logic();
	void setDrawMenusUnder(bool drawUnder) { this->drawUnder = drawUnder; }
	bool getDrawMenusUnder() const { return this->drawUnder; }

private:
	bool runLoop;
	int loopResult;
	gcn::Widget *oldtop;
	LuaActionListener *logiclistener;
	bool drawUnder;
};

#endif
