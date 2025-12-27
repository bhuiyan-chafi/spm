# Writing the Report

In this phase we will write the report for our project. I want you to do the following:

## Introduction

- this phase we will write the introduction
- first have a look at the project description @project1.pdf
- then review the code @src/openmp.cpp, farm.cpp, sequential.cpp, mpiff.cpp
- get to learn about the steps taken to solve the problem stated in the project description
- then go to the @src/discussions folder and read the markdown files
- those files will tell you what problems I have faced and how I have solved
- the course teaches about structured parallel programming, parallel paradigm models like pipeline and farm
- data parallelism map, stencil but I have used map only
- communication techniques
- single vs multi-node execution
- threading, openmp, fastflow
- theoretically it proves efficiency, scalability, speed, strong and weak scalability
- now this is what I want to write in the introduction:
-- what is the problem
-- why this problem is encountered mostly by the applications (database, searching and sorting)
-- how we addressed this problem in our project
-- why the technologies we used to solve the problem are most the efficient ones
-- how two different technologies openmp and fastflow solves the problem very efficiently
-- since I am a student I may not be able the best performance so find ways to make it more efficient as future scope to work
-- the fastflow framework is developed by professor massimo torquati to whom I am going to present the work
-- I achieved similar result (5second in openmp and 6second in fastflow) but I did so many things in openmp which I didn't have to do in fastflow
-- so I guess if I do more optimization I can achieve exact performance or even better than openmp
-- so I want you to highlight those scopes from the code and our workflow
-- the introduction must not be more then 1 page long
-- I am using Latex to write the report:
\usepackage[english]{babel}
\usepackage[utf8x]{inputenc}
\usepackage[T1]{fontenc}
%\usepackage{subfig}

%% Sets page size and margins
\usepackage[a4paper,top=2cm,bottom=2cm,left=1cm,right=1cm,marginparwidth=1.75cm]{geometry}

-- so try to know how many lines it can contain.
-- generate a new file in /report/introduction.txt and write everything there so that in future if needed we can work on that

==> some modifications after the first generation:

- avoid compound sentences and use simple english. My professor is italian and prefers simple grammatical structure
- since I am also a student, it should sound like a student describing rather than a professional
