To extract α, β and γ from a real parallel code, you need to **measure** (or reason out) two things:

1. **The cost of your “pure” computation** (α): how long does it take, on one processor, to do one “unit” of work—e.g. a single floating-point add or multiply?
2. **The cost of your communication or synchronization** (β): how long does it take to send one “unit” (or one message) between processors?

Once you have those,

$$
\gamma \;=\;\frac{\alpha}{\beta}
$$

is just their ratio.

---

## 1. Measuring α (compute cost)

1. **Identify your basic operation.**
   For a stencil code it might be “one grid-point update”; for an N-body it might be “one force calculation,” etc.

2. **Count how many of those ops are in your inner loop.**
   e.g.

   ```cpp
   // inner loop: u[i] = a*u[i-1] + b*v[i+1];
   //    = 1 multiply + 1 multiply + 1 add → 3 flops per element
   ```

3. **Time the loop in isolation.**

   ```cpp
   double t0 = MPI_Wtime();
   for(int i=1; i<N-1; ++i) {
     u[i] = a*u[i-1] + b*v[i+1];
   }
   double t1 = MPI_Wtime();
   ```

   * Let Δt = t1–t0.
   * You did `N-2` updates × 3 flops each = `3*(N-2)` flops.
   * So your **α** (sec per flop) ≈

     $$
       \alpha \;=\;\frac{Δt}{3\,(N-2)}.
     $$

---

## 2. Measuring β (communication cost)

1. **Pick a representative message size** (or unit): often you look at the largest halo you’ll send, or just “one message” if the model is size-independent.

2. **Ping-pong benchmark.**

   ```cpp
   const int tag = 17;
   std::vector<double> buf(M);  // M elements you normally send
   MPI_Barrier(MPI_COMM_WORLD);
   double t0 = MPI_Wtime();
   if(rank==0) {
     MPI_Send(buf.data(), M, MPI_DOUBLE, 1, tag, MPI_COMM_WORLD);
     MPI_Recv(buf.data(), M, MPI_DOUBLE, 1, tag, MPI_COMM_WORLD);
   } else if(rank==1) {
     MPI_Recv(buf.data(), M, MPI_DOUBLE, 0, tag, MPI_COMM_WORLD);
     MPI_Send(buf.data(), M, MPI_DOUBLE, 0, tag, MPI_COMM_WORLD);
   }
   double t1 = MPI_Wtime();
   ```

   * Δt = t1–t0 includes one round‐trip.
   * If you model β as “1 send” cost, then approximate

     $$
       β \;=\;\frac{Δt}{2}.
     $$
   * If you want a latency–bandwidth model, you can do this for various M, fit
     $\text{time}=L + M/B$, and take L as latency and 1/B as per-word cost.

---

## 3. Computing γ

Once you have

$$
α=\text{sec per flop (or per unit‐work)},\quad β=\text{sec per message},
$$

set

$$
γ=\frac{α}{β}.
$$

This dimensionless number tells you “how many ops I get for each message‐cost,” and it drives all of your scalability formulas.

---

## 4. What to write on an exam

> **“First I identify my fundamental compute‐unit (e.g. one grid update = 3 flops) and measure its cost in isolation with a tight loop and wall‐clock timers to get α = Δt\_wall / #ops.  Then I isolate my typical inter-PE message (or halo exchange) in a ping-pong benchmark, time its round‐trip and divide by two to estimate β = sec/message.  Finally γ = α/β measures the compute‐to-comm ratio that I plug into my theoretical speedup/scalability formulas.”**

That procedure will win you full credit—and if your assumptions (loop overhead, message size) match the exam’s, your α, β and γ will line up perfectly.
