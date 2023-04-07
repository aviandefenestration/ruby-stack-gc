# Fiber Benchmark

This benchmark demonstrates the cost of running multiple fibers and the overhead of garbage collection per fiber.

## Usage

Simply run the `fiber.rb` benchmark.

## Results

```
Count = 1
Count = 2
Count = 4
Count = 8
Count = 16
Count = 32
Count = 64
Count = 128
Count = 256
Count = 512
Count = 1024
Count = 2048
Count = 4096
Count = 8192
Count = 16384
GC 35 invokes.
Index    Invoke Time(sec)       Use Size(byte)     Total Size(byte)         Total Object                    GC Time(ms)
    1               0.022               766320              2096640                52416         0.70819400000000221063
    2               0.023               764160              2096640                52416         0.53977300000000028035
    3               0.024               804440              2096640                52416         0.55591300000000165582
    4               0.024               804400              2096640                52416         0.53746300000000191144
    5               0.025               884760              2096640                52416         0.58041300000000184411
    6               0.026               884760              2096640                52416         0.56753299999999851089
    7               0.027              1045400              2227680                55692         0.68527399999999960567
    8               0.027              1045400              2227680                55692         0.68218299999999942873
    9               0.029              1366680              2686320                67158         0.89450500000000032763
   10               0.030              1366680              2686320                67158         0.83200399999999730127
   11               0.033              2009240              3669120                91728         1.31506699999999621120
   12               0.034              2009240              3669120                91728         1.19577600000000261460
   13               0.040              3294360              5634720               140868         2.19901100000000049306
   14               0.042              3294360              5634720               140868         1.95834100000000232988
   15               0.053              5864600              9434880               235872         4.32589300000000420710
   16               0.057              5864600              9434880               235872         3.65531899999999732032
   17               0.079             11005080             17166240               429156         8.16038300000000660361
   18               0.087             11005080             17166240               429156         6.91902699999999448721
   19               0.129             21286040             32628960               815724        15.50100299999998476608
   20               0.145             21286040             32628960               815724        13.20321999999998752173
   21               0.228             41847960             63488880              1587222        29.57123799999997260102
   22               0.258             41847960             63488880              1587222        26.12055900000004271533
   23               0.424             82971800            125143200              3128580        57.95539900000002120350
   24               0.482             82971800            125143200              3128580        51.26594200000000967066
   25               0.820            165219480            248582880              6214572       115.00466300000000785531
   26               0.935            165219480            248582880              6214572       101.51354199999995842063
   27               1.614            329714840            495396720             12384918       229.99486599999997338273
   28               1.844            329714840            495396720             12384918       203.03655399999965425195
   29               3.193            658705560            989089920             24727248       469.21873300000038398139
   30               3.662            658705560            989089920             24727248       414.34203800000000228465
```

## Discussion

The benchmark runs 15 times, each time doubling the number of fibers. Each fiber recursively allocates a single object on the stack, 1000 times by default. The benchmark then runs the garbage collector twice. The first time, in theory, needs to do a full scan of the fiber stacks. However, since the fibers are not resumed or modified in any way, a full scan should not be necessary when invoking the garbage collector a second time.

The results include two rows for each iteration of the benchmark. Ideally, the 2nd row is significantly less than the first. Let's consider the last iteration of the benchmark, in which 16k fibers were allocated.

```
   29               3.193            658705560            989089920             24727248       469.21873300000038398139
   30               3.662            658705560            989089920             24727248       414.34203800000000228465
```

We can see that the time spent in the garbage collector is reduced, but still proportional to the number of fibers. Ideally, the 2nd run of the garbage collector can identify that the fibers' stacks have not been modified and do not need to be scanned again, thus the expectation is the 2nd run should be closer to O(1) in time, rather than proportional to the number of fibers.

## Ideas

### Modified List

Stacks which are "touched" between garbage collection could be tracked in a list. During garbage collection, only that list of fibers/threads should be scanned. In the best case, this reduces the overhead to zero, but in the worst case is still proportional to the number of stacks.

### Stack Pinning

Stacks which have objects allocated on them should pin those objects, e.g. in the old generation of the garbage collector. This would prevent the objects from being collected, even if the stacks were not scanned. In the best case, this reduces the overhead of scanning live but inactive stacks to zero, but in the worst case could stress the generational garbage collector as when stacks ARE modified, it would force extra book-keeping for the old generation.
