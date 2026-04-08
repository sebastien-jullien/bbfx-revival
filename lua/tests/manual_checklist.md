# BBFx v3.2.5 — Checklist de tests manuels

> Tests necessitant une interaction UI reelle (souris/clavier ImGui).
> A executer avant chaque release majeure.

## Node Editor — Multi-selection & Operations

- [ ] **U-013** Align Top/Bottom/Left/Right : multi-select 4+ noeuds → clic droit → Align → verifier alignement
- [ ] **U-014** Distribute H/V : multi-select 4+ noeuds → clic droit → Distribute → verifier espacement regulier
- [ ] **U-015** Node comment : clic droit noeud → Add Comment → saisir texte → bulle jaune visible
- [ ] **U-016** Node group : multi-select 3+ → Ctrl+G → rectangle colore avec titre visible
- [ ] **U-017** Ungroup : clic droit sur zone groupe → Ungroup → rectangle disparait, noeuds intacts
- [ ] **U-018** Collapse : clic droit noeud → Collapse → seuls les ports connectes visibles

## FX Stack

- [ ] **U-024** Reorder FX : dans Inspector Applied Effects, drag handle "::" → ordre change

## Crossfader A/B

- [ ] **U-031** Manual crossfade : en F5, slider A→B → visuels changent progressivement
- [ ] **U-032** Auto-crossfade : bouton Auto → slider se deplace sur N beats
- [ ] **U-033** Crossfade + automation : un port avec automation → crossfade ne l'ecrase pas

## Macro Triggers

- [ ] **U-040** Create macro : clic droit trigger → Edit Macro → ajouter 3 actions → OK
- [ ] **U-041** Execute macro : clic trigger avec macro → 3 actions executees en sequence
- [ ] **U-042** Macro wait : macro avec wait:2 → action suivante 2 beats plus tard

## Preset Wheel

- [ ] **U-050** Add to wheel : clic droit preset dans browser → Add to Wheel → segment visible
- [ ] **U-051** Load from wheel : clic segment → preset instancie
- [ ] **U-052** Remove from wheel : clic droit preset → Remove from Wheel → segment disparait

## Material Editor

- [ ] **U-060** Ouvrir : View → Material Editor → panel visible
- [ ] **U-061** Edit diffuse : changer couleur → viewport mis a jour en temps reel
- [ ] **U-062** New material : clic New Material → nommer → material cree et editable

## Shader Gallery

- [ ] **U-070** Ouvrir : View → Shader Gallery → 8 miniatures animees visibles
- [ ] **U-071** Apply : double-clic shader → ShaderFxNode cree + entity link auto
- [ ] **U-072** Drag : drag miniature vers viewport → meme resultat que double-clic

## Viewport & Camera

- [ ] **U-081** Camera FPS : clic droit maintenu + ZQSD → deplacement fluide
- [ ] **U-082** Gizmos : selectionner objet → drag fleche gizmo → position change + undo fonctionne
- [ ] **U-083** Hierarchy : drag objet sous un autre dans Scene Hierarchy → reparentage

## Timeline

- [ ] **U-084** Recording : REC → bouger fader → keyframes creees sur la lane

## Drag-Drop

- [ ] **U-086** Texture : drag texture depuis browser → objet viewport → TextureNode cree + lie
- [ ] **U-087** Material : drag material → objet → MaterialNode cree + lie

## Performance Mode

- [ ] **U-088** Pages : Tab → page suivante → triggers differents
- [ ] **U-089** Learn : bouton Learn sur fader → clic parametre Inspector → fader assigne

---

**Total : 29 tests manuels**
**Duree estimee : ~15 minutes**
