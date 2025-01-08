#!/bin/zsh

SEED=42
COUNT=10

RGG=../tools/random-game-generator

echo_and_run () {
  echo "$@"
  eval "$@"
}

[[ -x $RGG ]] || {
  echo "error: $RGG does not exist."
  echo "Run 'make' in the folder of the random game generator."
  exit 2
}

declare -A prio dens sizes

prio[high]="--maxp '2^size'"
prio[low]="--maxp 'size'"
dens[sparse]="--outdegree 2"
dens[dense]="--edges 'size * size * 0.2'"

sizes[sparse]={10000..50000..2500} # 20 steps
sizes[dense]={1000..5000..250}     # 20 steps

for prio_txt prio_arg in "${(@kv)prio}"; do
  for dens_txt dens_arg in "${(@kv)dens}"; do
    dir=$dens_txt-$prio_txt/
    mkdir -p $dir
    for sz in $(eval echo $sizes[$dens_txt]); do
      echo_and_run ../tools/random-game-generator --seed $SEED --count $COUNT --size $sz --bipartite --energy \
          ${=prio_arg} ${=dens_arg} $dir/$dens_txt-$prio_txt-sz=$sz-'{i}'.pg || exit
    done
  done
done
