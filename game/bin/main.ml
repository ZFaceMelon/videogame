let () =
  Printf.printf "videogame OCaml entry point (engine %s)\n%!" (Engine.version ());
  exit (Engine.run ())
