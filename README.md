# LED Bulletin Board
ESP32 controlled 64x32 LED matrix display panel, showing current time/date, news, affirmation & tasks.


## Current Layout
![VID_20220225_200431_01 (online-video-cutter com)](https://user-images.githubusercontent.com/30498489/155799221-f548ad4e-659d-454c-8de0-429894b17fa8.gif)

## Goal Layout
![pixil-frame-0(3)](https://user-images.githubusercontent.com/30498489/155784806-4140f807-80ad-4bb5-af04-aa672b66019f.png)
	
## Feature List
- [x] Research time/ date API.
  - [x] Get time/ date API working.
    - [x] Display current time & date on the panel.
    - [x] Create a ticker to trigger API after every (calculated) minute.
- [x] Add a celebration animation.
  - [x] For when work is done (17:30 detected).
    - [ ] Add a work ticker to avoid if statements.
  - [x] For lunch-time (13:00 detected).
    - [ ] Add a lunch ticker to avoid if statements.
- [x] Research affirmations API.
  - [x] Set up affirmations.dev API.
    - [x] Display affirmations on the panel.
    - [x] Create a rotating carousel to display the whole message.
- [x] Research News API.
  - [ ] Get News API working.
    - [ ] Display news on the panel.
    - [ ] Add news to the rotating carousel.
    - [ ] Set up a ticker to get the news every 15 mins.
    - [ ] Set up filter so as not to display the same stories multiple times a day.
- [ ] Research Weather API.
    - [ ] Add animation/ picture of current/projected weather.
- [ ] Create a list of de-stressing tasks/ To-Dos (eg. drink water, stretch, make tea, short walk, etc.).
    - [ ] Add animations counting down the time left to complete a task.
    - [ ] Decide on whether to randomise tasks or have a schedule.
        - [ ] Implement Decision. 
