# Program Development History

## The Genesis of the Program

### Encountering PLC

In April 2025, I first encountered PLC while preparing for the Semiconductor Equipment Maintenance Technician certification.
Having only worked with PC-related coding before, I was introduced to PLC code for the first time and learned ladder languages like LD, FBC, and ST.
![Semiconductor Equipment Work.jpg](../docs_img/%EB%B0%98%EC%84%A4%EB%B3%B4%20%EC%9E%91%EC%97%85.jpg)

However, there were too many students preparing for the certification compared to the number of available PLC workstations, so we had to take turns practicing.
Taking turns meant significantly reduced practice time, which made me think, "Why don't I just buy one myself?" But when I looked into it,
alas, PLCs were too expensive and difficult for individuals to purchase.
Eventually accepting this reality, I practiced intensively whenever my turn came, without even taking breaks.
However, without sufficient practice time, the exam became extremely challenging.
The PLC work itself wasn't difficult—it was the small mistakes from insufficient practice that made me barely finish on time.
PLC practice had a structural limitation: high equipment dependency made it difficult for individuals to engage in sufficient repetitive learning.
This experience led me to contemplate a solution that would allow adequate practice with PLC work.

### Program Development Performance Assessment

![Program Development Genesis.png](../docs_img/%ED%94%84%EB%A1%9C%EA%B7%B8%EB%9E%A8%20%EC%A0%9C%EC%9E%91%20%EB%B0%9C%EB%8B%A8.png)

My school had a programming course in the second year where we could learn the basics of C language.
The final performance assessment was an applied evaluation called "Program Development Using C Language."
I had previously participated in many SW competitions and had programming experience.
While thinking about ideas that could showcase my C language abilities and receive good evaluation in the assessment,
I remembered the solution I had contemplated during my previous PLC work:
**What if I could reproduce actual PLC work on a computer? Couldn't I practice PLC work on a computer even without a PLC?**
So, inspired by elements from PLC programming environments (GX Works2) and electronic component circuit simulation (TinkerCAD),
I started creating a **simulation program that could perform actual wiring, programming, and operation all in one program**.

## Starting MVP (Minimum Viable Product) Work

![Development Start.png](../docs_img/%EA%B0%9C%EB%B0%9C%20%EC%8B%9C%EC%9E%91.png)

First, although I had experience with C language development,
I had only created programs for hardware control like Arduino or microcontrollers,
and I struggled greatly with UI creation, having never made a GUI-based Windows program before.

So I first used AI to implement the elements I wanted to add, the UI, and basic operations using Claude's artifact feature.
[Actual Wiring Mode.tsx](%EC%8B%A4%EB%B0%B0%EC%84%A0%20%EB%AA%A8%EB%93%9C.tsx)
[Programming Mode.tsx](%ED%94%84%EB%A1%9C%EA%B7%B8%EB%9E%98%EB%B0%8D%20%EB%AA%A8%EB%93%9C.tsx)

Based on this completed UI, I created the actual C language-based program.
The day I completed this UI was 7 days before the performance assessment submission deadline.
It took a lot of time because of overlapping school schedules and **my inexperience in effectively using AI**.

And although it was clearly coded React UI code,
the actual process of creating it in C language was not so easy.

C language was vastly different from the TypeScript language used in React.
The difference in abstraction levels between C and TypeScript was significant,
and due to IMGUI, I was forced to compile using C++,
but **IMGUI couldn't be used with extern "C", so I had to rewrite the code in C++**—
I experienced many trial and error moments in completing the MVP.

Eventually, I submitted it in an incomplete state 5 minutes before the deadline,
but **the program was so heavy that the upload was delayed by 10 minutes,
causing me to miss the submission deadline and fail the performance assessment...**
![Program Submission.png](../docs_img/%ED%94%84%EB%A1%9C%EA%B7%B8%EB%9E%A8%20%EC%A0%9C%EC%B6%9C.png)

Fortunately, my teacher was lenient and I didn't lose points for late submission, but the program didn't reach completion,
and I was disappointed at failing to achieve the perfect score I had aimed for in the performance assessment.

Other students and teachers who knew I had worked hard on this program encouraged me,
but I felt great regret at not completing the program.

After feeling disappointed, since the incomplete MVP program still needed to be presented for the performance assessment,
I presented the program I had created to others.
![Post-MVP Development Presentation Experience.jpg](../docs_img/MVP%EA%B0%9C%EB%B0%9C%20%ED%9B%84%20%EB%B0%9C%ED%91%9C%20%EA%B2%BD%ED%97%98.jpg)
![MVP Program UI.png](../docs_img/MVP%20%ED%94%84%EB%A1%9C%EA%B7%B8%EB%9E%A8%20UI.png)

During the presentation, the response was not as good as I had expected.

The MVP program's completion level was lacking, and the overall atmosphere suggested that **people didn't really understand what the program did**.

Disappointingly, the MVP program's results were poor, and **this program was put aside due to exams and other things I was preparing...**

## Completely Overhauling the Program

### Problem Analysis

After the bitter failure and the end of exams and other preparations at the end of the semester,
**I listed the problems that existed in the previous MVP.**

1. Code was written without a clear style, making it difficult to modify.
2. Too much code was written in a single file.
3. Wiring and programming modes worked well separately but couldn't be connected.
4. Insufficient understanding of PLC operation caused many instances of behavior different from reality.
5. I used AI too stupidly.

There were more problems, but these were the five major ones.
To solve these problems, **I decided to refactor the entire codebase.**

### Determining Code Style

Although I had prepared the UI in advance during MVP,
the work that should have been done at the beginning of the project was insufficient, so the MVP code became a mess of dirty code.

To prevent this from happening again, **I first set the code style according to [GOOGLE CPP STYLE](https://google.github.io/styleguide/cppguide.html).**
Among various code styles, I thought the method used by Google, where various projects are conducted, was the most versatile
and suitable for this project, so I decided on it as the project's code style.

### Directory Structure

In the MVP, I wrote too much code in one file, which caused great difficulty in code writing.
So **I further subdivided the directory structure to make code modification more convenient**.

### New Physics Engine Design and OpenPLC Engine Integration

To connect the wiring and programming modes,
I contemplated the PLC connection method together with the physics engine.
**First, I boldly discarded the crude physics engine from the MVP and redesigned the physics engine from scratch.**
I separated physical phenomena and **divided them into electrical, pneumatic, and mechanical, making these three phenomena operate separately**
to **change them to be more realistic without complex operation**.

I also changed the method of interpreting PLC ladder in programming mode and applying it to the wiring environment.
Previously, I tried to apply the interpreted ladder directly to the wiring.
Ladder interpretation was a problem, but various bugs and performance issues occurred during the process of applying it to wiring.
Although I didn't skip studying PLC completely, I wasn't at a level to create a ladder interpreter and executor, and it would take too long to make,
so **based on the ladder interpreter and compiler from [OpenPLC Editor](https://github.com/thiagoralves/OpenPLC_Editor), one of the PLC-related open source projects**,
I tuned it to fit the program and created the following structure:
**Programming mode ladder writing -> Ladder interpretation -> Conversion to C++ code -> Emulating in wiring environment to run C++ program -> PLC operation**
This structure **configured an environment similar to actual PLC operation**.

### Efficient AI Usage with MD Syntax and Detailed Plans

Previously, when using AI, I didn't write detailed plans and always used it like:
implement xx, xx doesn't work, compilation error
in this manner.

**I painfully realized while creating the MVP that this method doesn't work as I want.**
So **I used AI more efficiently by writing MD syntax and detailed plans and reviewing them multiple times**.

[REFACTORING_PLAN.md](../legacy/docs/.copilot/REFACTORING_PLAN.md)
[DIRECTORY_STRUCTURE.md](../legacy/docs/.copilot/DIRECTORY_STRUCTURE.md)

Through these structural improvements, **I completely refactored the project in just 2 weeks**,
solving the **structural problems that occurred in the previous MVP and developing it into an actually operational PLC simulation program**.

## Teacher Feedback and Adding New Features and Improving Completeness

After the refactoring ended and a new semester started, there were many other things besides this project until the end of the semester, so a long time passed.
At the end of the semester, I started the project again and began receiving feedback from teachers.

Common feedback from PLC expert teachers and other teachers was
to add not only Semiconductor Equipment Maintenance Technician parts but also Automated Equipment Technician parts.

### Adding Automated Equipment Parts

![Automated Equipment Technician Parts.png](../docs_img/%EC%9E%90%EB%8F%99%ED%99%94%EC%84%A4%EB%B9%84%20%EA%B8%B0%EB%8A%A5%EC%82%AC%20%EB%B6%80%ED%92%88.png)

I thought the parts for Semiconductor Equipment Maintenance Technician and Automated Equipment Technician weren't very different,
but when I looked into it, they were quite different. First, there weren't just flat parts,
but machining cylinders and workpiece detection sensors, so the problem was how to make 3D parts operate on 2D without feeling out of place.

First, I proceeded with design to implement the parts in 2D.
I came up with techniques to create a sense of depth in 2D and tried to create depth with shadows and layer functions.
![Additional Parts Design Draft.png](../docs_img/%EC%B6%94%EA%B0%80%20%EB%B6%80%ED%92%88%20%EB%94%94%EC%9E%90%EC%9D%B8%20%EC%B4%88%EC%95%88.png)

Then, based on the design draft, I added new parts and implemented the functions of each part.
![Completed Parts.png](../docs_img/%EC%99%84%EC%84%B1%EB%90%9C%20%EB%B6%80%ED%92%88.png)

Also, since parts were only in one direction, I added a part rotation function so parts could be placed in various directions,
and I completely modified the mechanical physics engine so that workpieces would react with parts, allowing workpieces to be moved by various mechanical parts.
![Rotation.png](../docs_img/%ED%9A%8C%EC%A0%84.png)
![Workpiece Movement Implementation.gif](../docs_img/%EA%B3%B5%EC%9E%91%EB%AC%BC%20%EC%9D%B4%EB%8F%99%20%EA%B5%AC%ED%98%84.gif)

While adding parts, I implemented additional commands in programming mode. (SET RST BKRST)
![New Command Addition.png](../docs_img/%EC%83%88%EB%A1%9C%EC%9A%B4%20%EB%AA%85%EB%A0%B9%20%EC%B6%94%EA%B0%80.png)

### UI/UX Redesign

I redesigned the inconvenient and unclean UI and UX to be more comfortable and usable.
First, I changed the font from the default font to Pretendard font,
and added a filter function to the parts list and the ability to switch between icon view and title view to make finding parts faster.
![Old UI 1.png](../docs_img/%EA%B5%AC%EB%B2%84%EC%A0%84%20UI%201.png) ![New UI 1.png](../docs_img/%EC%8B%A0%EB%B2%84%EC%A0%84%20UI%201.png) ![New UI 2.png](../docs_img/%EC%8B%A0%EB%B2%84%EC%A0%84%20UI%202.png)

In programming mode, like GX Works2, I added shortcuts and ladder correction functions so it could be operated with the keyboard alone.
![Shortcuts.png](../docs_img/%EB%8B%A8%EC%B6%95%ED%82%A4.png)

I also enabled monitoring and testing of PLC operation.
![Monitoring.gif](../docs_img/%EB%AA%A8%EB%8B%88%ED%84%B0%EB%A7%81.gif)
![Testing.gif](../docs_img/%ED%83%9C%EC%8A%A4%ED%8A%B8.gif)

In wiring mode, opposite to programming, I made it possible to do all work with the mouse.
Drawing wiring, deleting, bringing and deleting parts, changing layer order, rotating, etc.—
I added mouse operations as intuitively as possible.
Of course, I also added shortcuts to the keyboard to maximize usability.
![Mouse Operation.gif](../docs_img/%EB%A7%88%EC%9A%B0%EC%8A%A4%20%EC%A1%B0%EC%9E%91.gif)
![Mouse Operation 2.gif](../docs_img/%EB%A7%88%EC%9A%B0%EC%8A%A4%20%EC%A1%B0%EC%9E%91%202.gif)

### Wire Embedding Feature

I added a wire embedding feature to hide wiring and quickly determine how things are connected.
![Wire Embedding Feature.gif](../docs_img/%EB%B0%B0%EC%84%A0%20%EC%9E%84%EB%B2%A0%EB%94%A9%20%EA%B8%B0%EB%8A%A5.gif)
![Wire Embedding Feature 2.gif](../docs_img/%EB%B0%B0%EC%84%A0%20%EC%9E%84%EB%B2%A0%EB%94%A9%20%EA%B8%B0%EB%8A%A5%202.gif)

### Performance Optimization

As features were added and parts increased, especially as complex mechanisms from **Automated Equipment Technician** were added, the program became increasingly heavy.
When multiple sensors, cylinders, and lots of wiring operated simultaneously, lag occurred, and I thought this would diminish its value as a simulation.

While **maintaining the structure of the physics engine I had built**, I proceeded with optimization by maintaining calculation results while reducing intermediate calculations to make it run more smoothly.

1. LOD System
   I made **the precision of physics calculations vary according to the Zoom value** of the canvas camera.
   When zoomed in close, it calculates even minute movements to show smooth animation.
   When zoomed out, since you only need to grasp the overall flow, I adjusted the physics calculation cycle to reduce CPU load.

2. View Frustum Culling
   No matter how many parts there are, the screen the user is looking at is limited. So at the rendering stage, I made it so that **parts and wiring outside the screen completely skip rendering calculations**.
   Thanks to this, even with hundreds of parts, I reduced graphic processing to ensure stable program operation.

3. Spatial Partitioning and Box2D Optimization
   When calculating collisions between workpieces and conveyors or cylinders, checking all parts one by one was too slow.
   To solve this, I **divided the space into a Grid and only performed collision checks with nearby parts**. Also, I excluded stationary workpieces from calculations to eliminate unnecessary operations.
   Additionally, workpiece collision and sensor recognition were handled by the open source Box2D physics engine, changing to more efficient collision calculations,
   which solved the throttling phenomenon that occurred in continuous collisions.

4. Multithreading
   I implemented multithreading so that **state updates of unrelated parts (electrical signals, valve logic, etc.) are processed in parallel by multiple threads**.
   As the number of parts increased, I could see significant gains in processing speed.

5. Memory Pooling
   For objects like electrical nodes or pneumatic edges that are frequently created and destroyed inside the physics engine,
   I applied a **memory pooling technique that pre-allocates memory and reuses it** to minimize runtime overhead.

With this optimization, what used to drop to 6fps during physical work
reached up to 400fps when frame limiting was removed in a low-spec environment with i5 3470 DDR3 8GB GT1030.
![Operating State (Frame Limited).jpg](../docs_img/%EC%9E%91%EB%8F%99%EC%83%81%ED%83%9C%28%ED%94%84%EB%A0%88%EC%9E%84%20%EC%A0%9C%EC%95%88%29.jpg)
![Operating State (No Frame Limit).jpg](../docs_img/%EC%9E%91%EB%8F%99%EC%83%81%ED%83%9C%28%ED%94%84%EB%A0%88%EC%9E%84%20%EC%A0%9C%EC%95%88%20X%29.jpg)
![IDLE State.jpg](../docs_img/IDLE%20%EC%83%81%ED%83%9C.jpg)

After refactoring, rather than adding major features, I focused more on taking care of details.
This work took more time and required more thought.

## In Conclusion

It's been a while since I did a project by myself.
Projects with teammates are usually for competitions,
so they were very crude works far from the completion I envisioned, but creating without thinking about deadlines
has finally produced a work close to the completion I imagine, which makes me very happy personally.
Projects with teammates were more about realizing ideas rather than being coding-centric,
whereas this project was coding-centric, resolving inconveniences I actually experienced,
while learning new technologies and theories through experience, seeming like a battle with myself to complete it to the end.
It was a project where I could create freely in what I do best.
Although I wasn't completely without help from others, I wrote the code alone and even did the design alone.
The project will be released as open source, and I probably won't have time to continue with part additions and other updates unless
an amazing pull request comes along or when I have time.
After that, it would probably deviate too much from this program's purpose.
That's it. The end!
