#RUN: %fish --features no-stderr-nocaret -c 'ls /abavojijsdfhdsjhfuihifoisj ^&1'
# CHECK: ls: {{.*}}
