#include <ardukit.h>

#include <Adafruit_Circuit_Playground.h>
#include <Adafruit_CircuitPlayground.h>

using namespace adk;

# define LED_PIN 12
# define LED_COUNT 40
# define Z_THRESHOLD 9.5
# define LEFT_ARM_LENGTH 8
# define LEFT_LEG_LENGTH 12
# define RIGHT_LEG_LENGTH 12
# define RIGHT_ARM_LENGTH 8
# define TOTAL_PIXELS (LEFT_ARM_LENGTH + LEFT_LEG_LENGTH + RIGHT_LEG_LENGTH + RIGHT_ARM_LENGTH)
# define MAX_FRAMES (4 * max(max(LEFT_ARM_LENGTH, LEFT_LEG_LENGTH), max(RIGHT_LEG_LENGTH, RIGHT_ARM_LENGTH)))
# define NUM_SPARKLE_PIXELS 5
# define DEFAULT_MODE ApplicationMode::SingleVertical
# define DEFAULT_ANIMATION_INTERVAL 100
# define DEFAULT_ANIMATION_INITIAL_DELAY 5000


float Z;
Adafruit_CPlay_NeoPixel strip;

enum Transition {None = 0, Appearing = 1, Disappearing = 2} state; 

enum class PixelIntensity {
  Off = 0,
  Light = 63,
  Medium = 127,
  Dark = 191,
  Full = 255,
};

enum class Appendage {
  LeftArm = 0,
  LeftLeg = 1,
  RightLeg = 2,
  RightArm = 3,
};

class PixelColor {
  unsigned short m_red;
  unsigned short m_green;
  unsigned short m_blue;
public:
  PixelColor() {};
  PixelColor(int r, int g, int b) : m_red(r), m_green(g), m_blue(b) {};
  void set_color(int r, int g, int b);
  unsigned short red();
  unsigned short green();
  unsigned short blue();
};

void PixelColor::set_color(int r, int g, int b) {
  m_red = r;
  m_green = g;
  m_blue = b;
}

unsigned short PixelColor::red() {
  return m_red;
}

unsigned short PixelColor::green() {
  return m_green;
}

unsigned short PixelColor::blue() {
  return m_blue;
}

class PixelSparkleAnimation : public Linkable {
    const int MAP_LENGTH = 8;
    static PixelIntensity PIXEL_INTENSITY_MAP[8];
    int m_frame;
  public:
    static int count;
    int id;
    PixelSparkleAnimation();
    PixelSparkleAnimation(int i);
    ~PixelSparkleAnimation();
    int increment_frame();
    PixelIntensity intensity();
    bool is_complete();
    void reset();
    operator Linkable*() const { return (Linkable*)this; }
};

int PixelSparkleAnimation::count = 0;

PixelIntensity PixelSparkleAnimation::PIXEL_INTENSITY_MAP[] = {
  PixelIntensity::Light,
  PixelIntensity::Medium,
  PixelIntensity::Dark,
  PixelIntensity::Full,
  PixelIntensity::Dark,
  PixelIntensity::Medium,
  PixelIntensity::Light,
  PixelIntensity::Off
};

PixelSparkleAnimation::PixelSparkleAnimation() {
  PixelSparkleAnimation(-1);
  m_frame = 0;
}

PixelSparkleAnimation::PixelSparkleAnimation(int i) : id(i) {
  m_frame = 0;
  count++;
}

PixelSparkleAnimation::~PixelSparkleAnimation()  {
  count--;
}

int PixelSparkleAnimation::increment_frame() {
  m_frame++;
  return m_frame;
}

PixelIntensity PixelSparkleAnimation::intensity() {
  int f = m_frame % MAP_LENGTH;
  return PixelSparkleAnimation::PIXEL_INTENSITY_MAP[f];
}

bool PixelSparkleAnimation::is_complete() {
  return m_frame >= MAP_LENGTH;
}

void PixelSparkleAnimation::reset() {
  m_frame = 0;
}


class Model {
  unsigned int transition_delay = 100;
  Transition transition;
  short masked_pixels = TOTAL_PIXELS;
  void __transition_task(Task &t);
  int num_sparkle_pixels = 0;
  PixelSparkleAnimation sparkle_pixels[NUM_SPARKLE_PIXELS] = {};
  void select_pixels_for_animation();
  PixelIntensity clamp_pixel_intensity(PixelSparkleAnimation& pixel);
  List<PixelSparkleAnimation> s_pixels;
  PixelColor m_default_color;

public:
  Model() {
    transition = None;
    m_default_color = PixelColor(255, 0, 128); // start with pink
  }

  static void task_animate_sparkles(void*);
  void __sparkle_task();
  void set_color(PixelColor c);

  void all(PixelIntensity intensity) {
    for (int i = 0; i < TOTAL_PIXELS; i++) {
      set_pixel(i, intensity);
    }
  }

  void set_transition(Transition t) {
    switch(transition) {
      case Appearing:
        masked_pixels = 0;
        break;
      case Disappearing:
        masked_pixels = TOTAL_PIXELS;
        break;
      default:
        masked_pixels = TOTAL_PIXELS;
        break;
    }

    transition = t;
  }

  int appear() {
    if (transition != None) {
      // transition not allowed
      return -1;
    }
    
    set_transition(Appearing);
    all(PixelIntensity::Off);
    // start transition task
    // sleep 8
    set_transition(None);
    return 0;
  }

  void disappear() {
    if (transition != None) {
      // transition not allowed
      return;
    }
    set_transition(Disappearing);
    all(PixelIntensity::Light);
    Task *t = Task::get_current();
    // start transition task
    // sleep 2
    set_transition(None);
  }
  
  void transition_frame(int idx, PixelIntensity intensity) {
    dmsg("transition_frame(%d, %d)\n", idx, (int)intensity);
    // map idx to appendage and actual pixel id
    /*
     * Assume a grid of 4 columns, one for each appendage. Some
     * appendages might be shorter than others, but we still
     * consider those pixels for simplicity. When a "ghost"
     * pixel is updated, we ignore it.
     */
    Appendage appendage = (Appendage)(idx % 4);
    int i = idx / 4;
    transition_appendage(appendage, i, intensity);
  }
  
  void transition_appendage(Appendage appendage, int idx, PixelIntensity i) {
    dmsg("transition_appendage: %d, %d, %d\n", (int)appendage, idx, (int)i);
    int pixel = 0;
    switch(appendage) {
      case Appendage::LeftArm:
        if (idx >= LEFT_ARM_LENGTH) {
          return;
        }
        pixel = idx;
        break;
      case Appendage::LeftLeg:
        if (idx >= LEFT_LEG_LENGTH) {
          return;
        }
        pixel = idx + LEFT_ARM_LENGTH;
        break;
      case Appendage::RightLeg:
        if (idx >= RIGHT_LEG_LENGTH) {
          return;
        }
        pixel = idx + LEFT_ARM_LENGTH + LEFT_LEG_LENGTH;
        break;
      case Appendage::RightArm:
        if (idx >= RIGHT_ARM_LENGTH) {
          return;
        }

        pixel = idx + LEFT_ARM_LENGTH + LEFT_LEG_LENGTH + RIGHT_LEG_LENGTH;
        break;
    }

    set_pixel(pixel, i);
  }

  void set_pixel(uint16_t idx, PixelIntensity i) {
    uint8_t red = m_default_color.red();
    uint8_t green = m_default_color.green();
    uint8_t blue = m_default_color.blue();

    // adjust for intensity
    red = min((int)((float)red * (int)i / 255), 255);
    green = min((int)((float)green * (int)i / 255), 255);
    blue = min((int)((float)blue * (int)i / 255), 255);
    dmsg("set_pixel(idx=%d, r=%d, g=%d, b=%d)\n", idx, red, green, blue);
    strip.setPixelColor(idx, red, green, blue);
  }
} model;


void Model::__sparkle_task() {
  //select_pixels_for_animation();
  PixelSparkleAnimation *last = s_pixels.first();
  if (!last) {
    s_pixels.append(new PixelSparkleAnimation(0));
  } else if (last->id < (20 -1)) {
    s_pixels.append(new PixelSparkleAnimation(last->id + 1));
  }

  dmsg("Animiating pixel count: %d\n", s_pixels.length());
  for (auto p = s_pixels.begin(); p != s_pixels.end(); p++) {
    Serial.print(p->id);
    Serial.print(" ");
  }
  Serial.println();

  for (auto p = s_pixels.begin(); p != s_pixels.end(); p++) {
    dmsg("loop %d\n", p->id);
    transition_frame(p->id, clamp_pixel_intensity(&*p));
    p->increment_frame();
  }

  // remove any completed animations
  auto p = s_pixels.begin();
  while (p != s_pixels.end()) {
    if (!p->is_complete()) {
      p++;
      continue;
    }

    dmsg("animation complete: %d\n", p->id);

    // increment the iterator so we don't lose it
    PixelSparkleAnimation *tmp = &*p++;
    // delete will detach the Linkable
    delete tmp;
  }

  dmsg("animating pixels count: %d\n", s_pixels.length());
}

PixelIntensity Model::clamp_pixel_intensity(PixelSparkleAnimation &pixel) {
  PixelIntensity intensity = pixel.intensity();
  if (transition != Transition::Appearing) {
    return intensity;
  }

  if (pixel.id >= masked_pixels) {
    return intensity;
  }

  if (intensity == PixelIntensity::Off) {
    return PixelIntensity::Light;
  }

  return intensity;
}

void Model::select_pixels_for_animation() {
  // each frame, add a random pixel, up to a maximum of NUM_SPARKLE_PIXELS total pixels
  if (s_pixels.length() >= NUM_SPARKLE_PIXELS) {
    return;
  }

  // pick a random pixel id
  int id = (int)random(TOTAL_PIXELS);
  dmsg("select_pixels_for_animation random id: %d\n", id);
  // avoid sparkling pixels above the mask
  if (id >= masked_pixels) {
    return;
  }

  // check the pixel hasn't already been selected
  for (auto p = s_pixels.begin(); p != s_pixels.end(); p++) {
    if (id == p->id) {
      dmsg("pixel already animating: %d\n", id);
      return;
    }
  }

  dmsg("appending pixel: %d\n", id);
  s_pixels.append(new PixelSparkleAnimation(id));
}

void Model::__transition_task(Task &t) {
  int total_pixels = TOTAL_PIXELS;
  PixelIntensity intensity;
  int direction, start, end;
  switch(transition) {
    case Appearing:
      intensity = PixelIntensity::Light;
      direction = 1; // animate top to bottom
      start = 0;
      end = total_pixels;
      masked_pixels = start;
      break;
    case Disappearing:
      intensity = PixelIntensity::Off;
      direction = -1; // animate bottom to top
      start = total_pixels - 1;
      end = -1;
      masked_pixels = total_pixels;
      break;
    default:
      // nothing to do
      return 0;
  }

  int idx = start;
  while (idx != end) {
      transition_frame(idx, intensity);
      t.sleep(transition_delay);
      idx += 1 * direction;
      masked_pixels += 1 * direction;
  }
}

void Model::task_animate_sparkles(void *m) {
  static_cast<Model*>(m)->__sparkle_task();
}


class TaskAnimateSingleSparkle : public Task {
  int m_id;
  PixelSparkleAnimation *m_pixel;
public:
  TaskAnimateSingleSparkle() {
    TaskAnimateSingleSparkle(0);
  }
  TaskAnimateSingleSparkle(int i) : m_id(i) {
    m_pixel = new PixelSparkleAnimation(m_id);
  }
  ~TaskAnimateSingleSparkle() {
    delete m_pixel;
  }
  virtual void run();
};

void TaskAnimateSingleSparkle::run() {
  dmsg("pixel: %d\n", m_pixel);
  model.transition_frame(m_id, m_pixel->intensity());
  int frame = m_pixel->increment_frame();
  dmsg("frame: %d\n", frame);
  dmsg("count: %d\n", PixelSparkleAnimation::count);
}




class TaskAnimateAllPixels : public Task {
  PixelSparkleAnimation *m_pixel = 0;
public:
  TaskAnimateAllPixels();
  virtual void run();
};

TaskAnimateAllPixels::TaskAnimateAllPixels() {
  m_pixel = new PixelSparkleAnimation(0);
  set_interval(DEFAULT_ANIMATION_INTERVAL);
}

void TaskAnimateAllPixels::run() {
  dmsg("animating pixel: %d\n", m_pixel->id);
  dmsg("count: %d\n", PixelSparkleAnimation::count);
  model.transition_frame(m_pixel->id, m_pixel->intensity());
  dmsg("animating pixel: %d\n", m_pixel->id);
  m_pixel->increment_frame();

  if (m_pixel->is_complete()) {
    dmsg("pixel complete: %d\n", m_pixel->id);
    int id = m_pixel->id;
    delete m_pixel;
    m_pixel = new PixelSparkleAnimation(++id % MAX_FRAMES);
  }

  strip.show();
}


class TaskAnimateSingleVertical : public Task {
  PixelSparkleAnimation m_pixel;
  Appendage m_appendage;
  unsigned short m_idx;
public:
  //TaskAnimateSingleVertical();
  virtual void run();
};

/*
TaskAnimateSingleVertical::TaskAnimateSingleVertical() {
  Task();
  set_interval(DEFAULT_ANIMATION_INTERVAL);
}
*/

void TaskAnimateSingleVertical::run() {
  int appendage_length;
  Appendage next_appendage;
  switch (m_appendage) {
    case Appendage::LeftArm:
      appendage_length = LEFT_ARM_LENGTH;
      next_appendage = Appendage::LeftLeg;
      break;
    case Appendage::LeftLeg:
      appendage_length = LEFT_LEG_LENGTH;
      next_appendage = Appendage::RightLeg;
      break;
    case Appendage::RightLeg:
      appendage_length = RIGHT_LEG_LENGTH;
      next_appendage = Appendage::RightArm;
      break;
    case Appendage::RightArm:
      appendage_length = RIGHT_ARM_LENGTH;
      next_appendage = Appendage::LeftArm;
      break;
    default:
      // unknown appendage?
      appendage_length = 0;
      next_appendage = Appendage::LeftArm;
      break;
  }

  dmsg("task horizontal start: %d, %d, %d\n", (int)m_appendage, m_idx, (int)m_pixel.intensity());

  model.transition_appendage(m_appendage, m_idx, m_pixel.intensity());
  m_pixel.increment_frame();
  if (m_pixel.is_complete()) {
    m_pixel.reset();
    m_idx++;
  }

  if (m_idx >= appendage_length) {
    m_idx = 0;
    m_appendage = next_appendage;
  }

  dmsg("task horizontal end: %d, %d, %d\n", (int)m_appendage, m_idx, (int)m_pixel.intensity());
  strip.show();
}


class TaskAnimateSparkles : public Task {
  PixelSparkleAnimation m_animating_pixels[NUM_SPARKLE_PIXELS];
  void select_pixels_for_animation();
  bool is_animating(const int idx);
public:
  virtual void run();
};

bool TaskAnimateSparkles::is_animating(const int idx) {
  const int len = NUM_SPARKLE_PIXELS;
  for (int i = 0; i < len; i++) {
    if (m_animating_pixels[i].id == idx) {
      return true;
    }
  }

  return false;
}

void TaskAnimateSparkles::select_pixels_for_animation() {
  // remove complete pixels
  const int len = NUM_SPARKLE_PIXELS;
  for (int i = 0; i < len; i++) {
    PixelSparkleAnimation *p = m_animating_pixels[i];
    if (p->id == -1) {
      continue;
    }
    
    if (p->is_complete()) {
      p->id = -1;
      p->reset();      
    }
  }


  // pick a random pixel id
  int id = (int)random(TOTAL_PIXELS);
  if (is_animating(id)) {
    return;
  }

  // TODO check masked pixels

  // find a slot for it
  for (int i = 0; i < len; i++) {
    PixelSparkleAnimation *p = m_animating_pixels[i];
    if (p->id == -1) {
      p->id = id;
      break;
    }
  }
}


void TaskAnimateSparkles::run() {
  select_pixels_for_animation();

  for (int i = 0; i < NUM_SPARKLE_PIXELS; i++) {
    PixelSparkleAnimation *p = m_animating_pixels[i];
    if (p->id == -1) {
      continue;
    }
    
    model.transition_frame(p->id, p->intensity());
    p->increment_frame();
  }

  strip.show();
}

// ------------------------------------------------------------

/*
TaskAnimateSingleSparkle task_animate_single_sparkle {1};
TaskAnimateSingleVertical task_animate_single_vertical;
TaskAnimateSparkles task_animate_sparkles;
TaskAnimateAllPixels task_animate_all_pixels;
Task sparkle_animation;
*/

enum class ApplicationMode {
  Normal = 0,
  SingleVertical = 1,
  SingleHorizontal = 2
} mode;


void __show_pixels(void*) {
  strip.show();
}


void switch_mode(void*) {
  switch (mode) {
    case ApplicationMode::Normal:
      break;
  }

  strip.clear();
}

void print_version(void*) {
  Serial.println("starting v2...");
}

Task *current_task = nullptr;
//TaskAnimateSingleVertical task_animate_single_vertical;
TaskAnimateSparkles task;

void setup() {
  delay(5000);
  Serial.begin(9600);
  CircuitPlayground.begin();  
  strip = Adafruit_CPlay_NeoPixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
  strip.begin();
  strip.clear();
  strip.setBrightness(40);
  state = None;
  randomSeed(analogRead(0));


  set_timeout(print_version, 5000);

  /*
  mode = DEFAULT_MODE;
  switch(mode) {
    case ApplicationMode::SingleVertical:
      current_task = new TaskAnimateSingleVertical();
      break;
    case ApplicationMode::SingleHorizontal:
    default:
      current_task = new TaskAnimateAllPixels();
      break;
  }*/
  
  /*
  current_task = new TaskAnimateSingleVertical();
  current_task->set_interval(500);
  current_task->start(DEFAULT_ANIMATION_INITIAL_DELAY);
  */

  task.set_interval(DEFAULT_ANIMATION_INTERVAL).start(DEFAULT_ANIMATION_INITIAL_DELAY);

  //task_animate_single_vertical.start(DEFAULT_ANIMATION_INITIAL_DELAY);

  //adk::set_interval(check_z, 200);
  //model.sparkle_animation();
  //sparkle_animation.set_interval(1000).start(&Model::task_animate_sparkles, &model);
  //task_animate_single_sparkle.set_interval(500).start(1);
  //task_animate_sparkles.set_interval(500).start(1);
  
  //adk::set_interval(__show_pixels, 100);
  //adk::set_interval(task_one_pixel, 1000);
}

void loop() {
  adk::run();
}

void task_one_pixel() {
  static int i = 0;
  
  strip.clear();
  model.transition_frame(i++, PixelIntensity::Full);
  strip.show();
  if (i >= TOTAL_PIXELS) {
    i = 0;
  }
}

void check_z() {
  Z = CircuitPlayground.motionZ();
  Serial.print("Z: ");
  Serial.println(Z);

  if (abs(Z) >= Z_THRESHOLD) {
    state = Z > 0 ? Appearing : Disappearing;
  } else {
    state = None;
  }
  Serial.print("state: ");
  Serial.println(state);

  switch (state) {
    case Appearing:
      model.appear();
      break;
    case Disappearing:
      model.disappear();
      break;
    default:
      break;
  }
}
