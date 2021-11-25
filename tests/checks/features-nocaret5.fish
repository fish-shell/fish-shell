#RUN: %fish --features no-stderr-nocaret -c 'cat /abavojijsdfhdsjhfuihifoisj ^&1'
# CHECK: {{cat|/abavojijsdfhdsjhfuihifoisj}}: {{.*}}
